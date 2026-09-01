"""Laptop-side verification tool for the Bedside Focus Badge.

Scans for the badge's BLE advertisement and prints focus state changes.
See docs/ble-protocol.md for the advertising format this parses.

Usage:
    uv run scripts/scan.py
"""

import asyncio

from bleak import BleakScanner
from bleak.backends.device import BLEDevice
from bleak.backends.scanner import AdvertisementData

DEVICE_NAME = "PresenceBadge"
MANUFACTURER_ID = 0xFFFF
PROTOCOL_MAGIC = 0x50

_last_state: bool | None = None


def _parse_state(mfg_bytes: bytes) -> bool | None:
    if len(mfg_bytes) < 2 or mfg_bytes[0] != PROTOCOL_MAGIC:
        return None
    return mfg_bytes[1] == 0x01


def _on_detection(device: BLEDevice, advertisement: AdvertisementData) -> None:
    global _last_state

    if advertisement.local_name != DEVICE_NAME:
        return

    mfg_bytes = advertisement.manufacturer_data.get(MANUFACTURER_ID)
    if mfg_bytes is None:
        return

    state = _parse_state(mfg_bytes)
    if state is None or state == _last_state:
        return

    _last_state = state
    print(f"Focus {'ON' if state else 'OFF'}  (badge: {device.address})")


async def main() -> None:
    print(f"Scanning for '{DEVICE_NAME}' advertisements... (Ctrl+C to stop)")
    async with BleakScanner(_on_detection):
        await asyncio.Event().wait()


if __name__ == "__main__":
    asyncio.run(main())
