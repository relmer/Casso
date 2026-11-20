#!/usr/bin/env python3
"""Relays frames between MAME's TAP device and a second TAP device.

MAME's LANceGS is on tap-mess-0-0; tap-wire has the host's address. Frames
to MAME are padded to 60 bytes and given a 4-byte CRC, as on a real wire,
since MAME's SMC91C9x drops a frame shorter than 64 bytes and frames from a
TAP device have no CRC.
"""

import fcntl, os, select, socket, struct, zlib

TUNSETIFF       = 0x400454CA
IFF_TAP         = 0x0002
IFF_NO_PI       = 0x1000
ETH_P_ALL       = 0x0003
PACKET_OUTGOING = 4

wire = os.open('/dev/net/tun', os.O_RDWR)
fcntl.ioctl(wire, TUNSETIFF, struct.pack('16sH', b'tap-wire', IFF_TAP | IFF_NO_PI))

mame = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(ETH_P_ALL))
mame.bind(('tap-mess-0-0', 0))

log = open('/root/netrun/wirepad.log', 'a', buffering=1)
log.write('relay started\n')

while True:
    ready, _, _ = select.select([wire, mame], [], [])

    if wire in ready:
        frame = os.read(wire, 2048)

        if frame[12:14] not in (b'\x08\x00', b'\x08\x06'):
            log.write(f'dropped type {frame[12:14].hex()}\n')
            continue

        if len(frame) < 60:
            frame = frame + bytes(60 - len(frame))

        frame = frame + struct.pack('<I', zlib.crc32(frame))
        mame.send(frame)
        log.write(f'to apple {len(frame)}\n')

    if mame in ready:
        frame, address = mame.recvfrom(2048)

        if address[2] == PACKET_OUTGOING:
            continue

        os.write(wire, frame)
        log.write(f'from apple {len(frame)}\n')
