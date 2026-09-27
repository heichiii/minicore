#!/usr/bin/env python3
"""Build the small deterministic newc initramfs used by MiniCore."""

import pathlib
import sys


def pad4(data: bytearray) -> None:
    data.extend(b"\0" * (-len(data) & 3))


def add_entry(archive: bytearray, name: str, payload: bytes, mode: int) -> None:
    encoded = name.encode("ascii") + b"\0"
    fields = [
        1, mode, 0, 0, 1, 0, len(payload), 0, 0, 0, 0, len(encoded), 0
    ]
    archive.extend(b"070701" + b"".join(f"{value:08x}".encode() for value in fields))
    archive.extend(encoded)
    pad4(archive)
    archive.extend(payload)
    pad4(archive)


def main() -> None:
    if len(sys.argv) < 3:
        raise SystemExit("usage: mkinitramfs.py OUTPUT NAME=FILE ...")
    archive = bytearray()
    add_entry(archive, ".", b"", 0o040755)
    add_entry(archive, "bin", b"", 0o040755)
    # Real directory and symlink fixtures exercise component traversal; link
    # payloads are paths, without the C-string terminator used for names.
    add_entry(archive, "etc", b"", 0o040755)
    add_entry(archive, "etc/message", b"abcdef\n", 0o100444)
    add_entry(archive, "link", b"etc/message", 0o120777)
    add_entry(archive, "abs-link", b"/etc/message", 0o120777)
    add_entry(archive, "dir-link", b"etc", 0o120777)
    add_entry(archive, "dangling", b"/missing", 0o120777)
    add_entry(archive, "loop", b"/loop", 0o120777)
    for item in sys.argv[2:]:
        name, filename = item.split("=", 1)
        add_entry(archive, name, pathlib.Path(filename).read_bytes(), 0o100755)
    add_entry(archive, "TRAILER!!!", b"", 0)
    pathlib.Path(sys.argv[1]).write_bytes(archive)


if __name__ == "__main__":
    main()
