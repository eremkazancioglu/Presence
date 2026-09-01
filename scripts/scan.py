"""Laptop-side verification tool for the Bedside Focus Badge.

Scans for the badge's iBeacon advertisement and prints focus state
changes. See docs/ble-protocol.md for the advertising format this parses.

Usage:
    uv run scripts/scan.py
"""

import asyncio
import struct
from uuid import UUID

from bleak import BleakScanner
from bleak.backends.device import BLEDevice
from bleak.backends.scanner import AdvertisementData

APPLE_MANUFACTURER_ID = 0x004C
IBEACON_SUBTYPE = 0x02
BADGE_UUID = UUID("651bbfec-f197-444e-bf25-d72c1d4ccd84")
MINOR_ON = 1

_last_state: bool | None = None


def _parse_minor(mfg_bytes: bytes) -> int | None:
    # Layout: subtype(1) subtype_length(1) uuid(16) major(2) minor(2) tx_power(1)
    if len(mfg_bytes) < 23 or mfg_bytes[0] != IBEACON_SUBTYPE:
        return None

    uuid = UUID(bytes=mfg_bytes[2:18])
    if uuid != BADGE_UUID:
        return None

    (minor,) = struct.unpack(">H", mfg_bytes[20:22])
    return minor


def _on_detection(device: BLEDevice, advertisement: AdvertisementData) -> None:
    global _last_state

    mfg_bytes = advertisement.manufacturer_data.get(APPLE_MANUFACTURER_ID)
    if mfg_bytes is None:
        return

    minor = _parse_minor(mfg_bytes)
    if minor is None:
        return

    state = minor == MINOR_ON
    if state == _last_state:
        return

    _last_state = state
    print(f"Focus {'ON' if state else 'OFF'}  (badge: {device.address})")


async def main() -> None:
    print("Scanning for the badge's iBeacon advertisement... (Ctrl+C to stop)")
    async with BleakScanner(_on_detection):
        await asyncio.Event().wait()


if __name__ == "__main__":
    asyncio.run(main())
