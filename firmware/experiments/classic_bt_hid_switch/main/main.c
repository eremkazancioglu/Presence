/*
 * Bedside Focus Badge -- EXPERIMENT, not the production firmware.
 *
 * Classic Bluetooth (BR/EDR) HID device, button-driven connect/disconnect,
 * on a plain ESP32-WROOM-32 (not the XIAO ESP32C6, which is BLE-only and
 * has no BR/EDR radio). This exists to test a device class we haven't
 * tried: Shortcuts' "Bluetooth device connects/disconnects" automation
 * fired reliably (even locked) for a real headphone accessory, and was
 * separately confirmed to fire for a DualShock controller -- both Classic
 * Bluetooth HID/audio profiles. Every earlier design used BLE-only
 * hardware, which that automation apparently never recognizes at all
 * (see docs/trigger-mechanism-investigation.md, design 6). This tests
 * whether presenting as a genuine Classic BT HID device sidesteps that
 * limitation entirely, without Full Keyboard Access and without any app.
 *
 * Behavior:
 *   1) First press (no bonded host yet): become connectable/discoverable
 *      so the phone can pair via Settings -> Bluetooth.
 *   2) Press while disconnected ("on"): actively reconnect to the bonded
 *      phone -- this connect event is what the Shortcuts automation
 *      reacts to.
 *   3) Press while connected ("off"): actively disconnect -- the
 *      disconnect event is what the automation's "disconnects" trigger
 *      reacts to.
 *   4) After a disconnect, deliberately stays non-connectable/
 *      non-discoverable -- no auto-reconnect until the next press.
 *
 * No actual HID report data matters here -- only the connect/disconnect
 * events themselves are the signal. Presents as a Keyboard-class HID
 * device (COD + descriptor), not the Mouse class this started as: a
 * "mouse" never appeared in Settings -> Bluetooth's Other Devices list at
 * all, while Keyboard is a class iOS unambiguously supports pairing (the
 * same class our working BLE badge.ino uses, and consistent with the
 * DualShock/headphone precedent that started this avenue -- both
 * keyboard-or-audio-adjacent profiles, not a generic pointing device).
 * Base structure adapted from Espressif's own bt_hid_mouse_device example
 * (bluetooth/bluedroid/classic_bt/bt_hid_mouse_device), swapping in a
 * standard USB HID Boot Keyboard descriptor and never running its
 * periodic report-sending task.
 *
 * Requires the full ESP-IDF toolchain (not Arduino) -- see
 * docs/ble-protocol.md and the top-level README for why, and how to
 * build this specifically.
 *
 * Board: plain ESP32-WROOM-32 (e.g. AITRIP ESP32 DevKit, CP2102, 30-pin).
 */

#include "esp_log.h"
#include "esp_hidd_api.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_bt.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_gap_bt_api.h"
#include "driver/gpio.h"
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "presence_switch";

static const char local_device_name[] = "PresenceBadge";

// TEMP: no external tactile button wired yet -- jumper this pin to GND to
// simulate a press, same convention as firmware/badge/badge.ino.
#define BUTTON_GPIO GPIO_NUM_4
#define DEBOUNCE_MS 50

// Keyboard report (Report Protocol Mode, no report ID declared in the
// descriptor): 1 modifier byte + 1 reserved byte + 6 keycode bytes = 8.
#define REPORT_PROTOCOL_KEYBOARD_REPORT_SIZE (8)
#define REPORT_BUFFER_SIZE REPORT_PROTOCOL_KEYBOARD_REPORT_SIZE

typedef struct {
    esp_hidd_app_param_t app_param;
    esp_hidd_qos_param_t both_qos;
    uint8_t protocol_mode;
    uint8_t buffer[REPORT_BUFFER_SIZE];
    bool has_bonded_host;
    esp_bd_addr_t bonded_addr;
    bool connected;
} local_param_t;

static local_param_t s_local_param = {0};

// Standard USB HID Boot Keyboard report descriptor (the same well-known
// descriptor shape used everywhere, including our BLE badge.ino). Switched
// from a "mouse" class/descriptor after that didn't show up in Settings ->
// Bluetooth at all -- iOS pairing support for Classic BT HID appears not
// to extend to generic pointing devices the same way it does keyboards
// (and game controllers, per the DualShock precedent that started this
// whole avenue). We never send real keypresses; only GET_REPORT queries
// get answered (with all-zero/idle data).
static uint8_t hid_keyboard_descriptor[] = {
    0x05, 0x01,       // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,       // USAGE (Keyboard)
    0xa1, 0x01,       // COLLECTION (Application)
    0x05, 0x07,       //   USAGE_PAGE (Keyboard/Keypad)
    0x19, 0xe0,       //   USAGE_MINIMUM (Left Control)
    0x29, 0xe7,       //   USAGE_MAXIMUM (Right GUI)
    0x15, 0x00,       //   LOGICAL_MINIMUM (0)
    0x25, 0x01,       //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,       //   REPORT_SIZE (1)
    0x95, 0x08,       //   REPORT_COUNT (8)
    0x81, 0x02,       //   INPUT (Data,Var,Abs) -- modifier byte
    0x95, 0x01,       //   REPORT_COUNT (1)
    0x75, 0x08,       //   REPORT_SIZE (8)
    0x81, 0x01,       //   INPUT (Cnst,Ary,Abs) -- reserved byte
    0x95, 0x05,       //   REPORT_COUNT (5)
    0x75, 0x01,       //   REPORT_SIZE (1)
    0x05, 0x08,       //   USAGE_PAGE (LEDs)
    0x19, 0x01,       //   USAGE_MINIMUM (Num Lock)
    0x29, 0x05,       //   USAGE_MAXIMUM (Kana)
    0x91, 0x02,       //   OUTPUT (Data,Var,Abs) -- LED report
    0x95, 0x01,       //   REPORT_COUNT (1)
    0x75, 0x03,       //   REPORT_SIZE (3)
    0x91, 0x01,       //   OUTPUT (Cnst,Ary,Abs) -- LED report padding
    0x95, 0x06,       //   REPORT_COUNT (6)
    0x75, 0x08,       //   REPORT_SIZE (8)
    0x15, 0x00,       //   LOGICAL_MINIMUM (0)
    0x25, 0x65,       //   LOGICAL_MAXIMUM (101)
    0x05, 0x07,       //   USAGE_PAGE (Keyboard/Keypad)
    0x19, 0x00,       //   USAGE_MINIMUM (Reserved)
    0x29, 0x65,       //   USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00,       //   INPUT (Data,Ary,Abs) -- key array (6 bytes)
    0xc0              // END_COLLECTION
};
static const int hid_keyboard_descriptor_len = sizeof(hid_keyboard_descriptor);

static char *bda2str(esp_bd_addr_t bda, char *str, size_t size)
{
    if (bda == NULL || str == NULL || size < 18) {
        return NULL;
    }
    uint8_t *p = bda;
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x", p[0], p[1], p[2], p[3], p[4], p[5]);
    return str;
}

/// Button press "on": actively connect to the bonded host if we have one,
/// or just go connectable/discoverable for a first-time pairing.
/// Button press "off": actively disconnect.
static void toggle_switch(void)
{
    if (!s_local_param.connected) {
        ESP_LOGI(TAG, "Switch ON");
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        if (s_local_param.has_bonded_host) {
            char bda_str[18] = {0};
            ESP_LOGI(TAG, "Connecting to bonded host %s", bda2str(s_local_param.bonded_addr, bda_str, sizeof(bda_str)));
            esp_bt_hid_device_connect(s_local_param.bonded_addr);
        } else {
            ESP_LOGI(TAG, "No bonded host yet -- pair via Settings > Bluetooth now.");
        }
    } else {
        ESP_LOGI(TAG, "Switch OFF -- disconnecting");
        esp_bt_hid_device_disconnect();
    }
}

static void button_task(void *pvParameters)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    int last_reading = 1;
    int debounced_state = 1;
    TickType_t last_debounce_tick = xTaskGetTickCount();

    for (;;) {
        int reading = gpio_get_level(BUTTON_GPIO);
        if (reading != last_reading) {
            last_debounce_tick = xTaskGetTickCount();
        }
        if ((xTaskGetTickCount() - last_debounce_tick) * portTICK_PERIOD_MS > DEBOUNCE_MS) {
            if (reading != debounced_state) {
                debounced_state = reading;
                // Active LOW: button pressed pulls the pin to GND.
                if (debounced_state == 0) {
                    toggle_switch();
                }
            }
        }
        last_reading = reading;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

static void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "authentication success: %s", param->auth_cmpl.device_name);
        } else {
            ESP_LOGE(TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
        }
        break;
    case ESP_BT_GAP_PIN_REQ_EVT:
        if (param->pin_req.min_16_digit) {
            esp_bt_pin_code_t pin_code = {0};
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
        } else {
            esp_bt_pin_code_t pin_code;
            pin_code[0] = '1';
            pin_code[1] = '2';
            pin_code[2] = '3';
            pin_code[3] = '4';
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
        }
        break;
    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(TAG, "ESP_BT_GAP_CFM_REQ_EVT, compare value: %06" PRIu32, param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%06" PRIu32, param->key_notif.passkey);
        break;
    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(TAG, "ESP_BT_GAP_MODE_CHG_EVT mode:%d", param->mode_chg.mode);
        break;
    default:
        ESP_LOGI(TAG, "gap event: %d", event);
        break;
    }
}

static void esp_bt_hidd_cb(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param)
{
    switch (event) {
    case ESP_HIDD_INIT_EVT:
        if (param->init.status == ESP_HIDD_SUCCESS) {
            ESP_LOGI(TAG, "hidd init ok, registering app");
            esp_bt_hid_device_register_app(&s_local_param.app_param, &s_local_param.both_qos, &s_local_param.both_qos);
        } else {
            ESP_LOGE(TAG, "hidd init failed");
        }
        break;
    case ESP_HIDD_DEINIT_EVT:
        break;
    case ESP_HIDD_REGISTER_APP_EVT:
        if (param->register_app.status == ESP_HIDD_SUCCESS) {
            // Deliberately stay non-connectable/non-discoverable at boot --
            // no auto-reconnect until the button is actually pressed. Just
            // remember whether Bluedroid already has a bonded host from a
            // previous pairing, for the next button press to use.
            ESP_LOGI(TAG, "app registered, in_use=%d", param->register_app.in_use);
            if (param->register_app.in_use) {
                s_local_param.has_bonded_host = true;
                memcpy(s_local_param.bonded_addr, param->register_app.bd_addr, sizeof(esp_bd_addr_t));
            }
            esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
        } else {
            ESP_LOGE(TAG, "register app failed");
        }
        break;
    case ESP_HIDD_UNREGISTER_APP_EVT:
        break;
    case ESP_HIDD_OPEN_EVT:
        if (param->open.status == ESP_HIDD_SUCCESS) {
            if (param->open.conn_status == ESP_HIDD_CONN_STATE_CONNECTING) {
                ESP_LOGI(TAG, "connecting...");
            } else if (param->open.conn_status == ESP_HIDD_CONN_STATE_CONNECTED) {
                char bda_str[18] = {0};
                ESP_LOGI(TAG, "connected to %s", bda2str(param->open.bd_addr, bda_str, sizeof(bda_str)));
                s_local_param.connected = true;
                s_local_param.has_bonded_host = true;
                memcpy(s_local_param.bonded_addr, param->open.bd_addr, sizeof(esp_bd_addr_t));
                esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
            }
        } else {
            ESP_LOGE(TAG, "open failed");
        }
        break;
    case ESP_HIDD_CLOSE_EVT:
        if (param->close.conn_status == ESP_HIDD_CONN_STATE_DISCONNECTING) {
            ESP_LOGI(TAG, "disconnecting...");
        } else if (param->close.conn_status == ESP_HIDD_CONN_STATE_DISCONNECTED) {
            ESP_LOGI(TAG, "disconnected -- staying non-discoverable until next press");
            s_local_param.connected = false;
            // Deliberately NOT re-enabling connectable/discoverable here
            // (unlike Espressif's example) -- requirement 4: no
            // auto-reconnect until the button is pressed again.
        }
        break;
    case ESP_HIDD_SEND_REPORT_EVT:
    case ESP_HIDD_REPORT_ERR_EVT:
        break;
    case ESP_HIDD_GET_REPORT_EVT:
        // Idle keyboard, all-zero report (no keys held) -- buffer is never
        // written to since we don't send real keystrokes.
        if (s_local_param.protocol_mode == ESP_HIDD_REPORT_MODE) {
            esp_bt_hid_device_send_report(ESP_HIDD_REPORT_TYPE_INPUT, 0, REPORT_PROTOCOL_KEYBOARD_REPORT_SIZE, s_local_param.buffer);
        } else {
            esp_bt_hid_device_send_report(ESP_HIDD_REPORT_TYPE_INPUT, ESP_HIDD_BOOT_REPORT_ID_KEYBOARD, ESP_HIDD_BOOT_REPORT_SIZE_KEYBOARD - 1, s_local_param.buffer);
        }
        break;
    case ESP_HIDD_SET_REPORT_EVT:
        break;
    case ESP_HIDD_SET_PROTOCOL_EVT:
        s_local_param.protocol_mode = param->set_protocol.protocol_mode;
        break;
    case ESP_HIDD_INTR_DATA_EVT:
        break;
    case ESP_HIDD_VC_UNPLUG_EVT:
        ESP_LOGI(TAG, "virtual cable unplugged -- staying non-discoverable until next press");
        s_local_param.connected = false;
        break;
    default:
        break;
    }
}

void app_main(void)
{
    esp_err_t ret;
    char bda_str[18] = {0};

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGE(TAG, "controller init failed: %s", esp_err_to_name(ret));
        return;
    }
    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
        ESP_LOGE(TAG, "controller enable failed: %s", esp_err_to_name(ret));
        return;
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    if ((ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg)) != ESP_OK) {
        ESP_LOGE(TAG, "bluedroid init failed: %s", esp_err_to_name(ret));
        return;
    }
    if ((ret = esp_bluedroid_enable()) != ESP_OK) {
        ESP_LOGE(TAG, "bluedroid enable failed: %s", esp_err_to_name(ret));
        return;
    }
    if ((ret = esp_bt_gap_register_callback(esp_bt_gap_cb)) != ESP_OK) {
        ESP_LOGE(TAG, "gap register failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "setting device name: %s", local_device_name);
    esp_bt_gap_set_device_name(local_device_name);

    esp_bt_cod_t cod = {0};
    cod.major = ESP_BT_COD_MAJOR_DEV_PERIPHERAL;
    cod.minor = ESP_BT_COD_MINOR_PERIPHERAL_KEYBOARD;
    esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_MAJOR_MINOR);

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    s_local_param.app_param.name = "PresenceBadge";
    s_local_param.app_param.description = "Presence Badge trigger switch";
    s_local_param.app_param.provider = "Presence";
    s_local_param.app_param.subclass = ESP_HID_CLASS_KBD;
    s_local_param.app_param.desc_list = hid_keyboard_descriptor;
    s_local_param.app_param.desc_list_len = hid_keyboard_descriptor_len;
    memset(&s_local_param.both_qos, 0, sizeof(esp_hidd_qos_param_t));
    s_local_param.protocol_mode = ESP_HIDD_REPORT_MODE;

    esp_bt_hid_device_register_callback(esp_bt_hidd_cb);
    esp_bt_hid_device_init();

    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code;
    esp_bt_gap_set_pin(pin_type, 0, pin_code);

    ESP_LOGI(TAG, "own address: %s", bda2str((uint8_t *)esp_bt_dev_get_address(), bda_str, sizeof(bda_str)));

    xTaskCreate(button_task, "button_task", 4 * 1024, NULL, 10, NULL);

    ESP_LOGI(TAG, "ready -- non-discoverable. Press the button to pair/connect.");
}
