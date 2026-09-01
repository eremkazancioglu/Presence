"""Laptop-side verification tool for the Bedside Focus Badge.

The badge is a BLE HID keyboard that stays continuously connected to the
phone once paired -- see docs/ble-protocol.md. It only advertises while
unpaired/disconnected (e.g. fresh out of setup, or if the phone is out of
range), so this is mainly useful as a pre-pairing sanity check that the
badge is alive and broadcasting -- not an ongoing state monitor, since
there's no state payload once it's paired and connected to the phone.

Usage:
    uv run scripts/scan.py
"""

import asyncio

from bleak import BleakScanner
from bleak.backends.device import BLEDevice
from bleak.backends.scanner import AdvertisementData

DEVICE_NAME = "PresenceBadge"
ABSENCE_TIMEOUT_S = 3.0

_seen = False
_last_seen_at = 0.0


def _on_detection(device: BLEDevice, advertisement: AdvertisementData) -> None:
    global _seen, _last_seen_at

    if advertisement.local_name != DEVICE_NAME:
        return

    _last_seen_at = asyncio.get_event_loop().time()
    if not _seen:
        _seen = True
        print(f"Badge advertising (wants to connect)  (badge: {device.address})")


async def _watch_for_absence() -> None:
    global _seen
    while True:
        await asyncio.sleep(0.5)
        if _seen and (asyncio.get_event_loop().time() - _last_seen_at) > ABSENCE_TIMEOUT_S:
            _seen = False
            print("Badge no longer advertising (connected, or off)")


async def main() -> None:
    print(f"Scanning for '{DEVICE_NAME}' advertisements... (Ctrl+C to stop)")
    async with BleakScanner(_on_detection):
        await _watch_for_absence()


if __name__ == "__main__":
    asyncio.run(main())
