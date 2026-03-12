# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

"""Protobuf read/write utilities."""

import struct

INTEGER_SIZE = 4
SIZE_STRUCT = struct.Struct("<L")


def is32(num_bytes: bytes) -> bool:
    """Check if bytes can be an integer."""
    return len(num_bytes) == INTEGER_SIZE


def read32(_input) -> int | None:
    """Get message size in bytes, encoded as bytes."""
    num_bytes = _input.read(INTEGER_SIZE)
    if not is32(num_bytes):
        return None
    return SIZE_STRUCT.unpack(num_bytes)[0]


def write32(value: int, output):
    """Write message size in bytes, encoded as bytes."""
    output.write(SIZE_STRUCT.pack(value))


def write_header_to(magic: int, version: int, output):
    """Write header (magic + version)."""
    write32(magic, output)
    write32(version, output)


def read_header_from(_input) -> tuple[int | None, int | None]:
    """Read header (magic + version)."""
    magic = read32(_input)
    version = read32(_input)
    return (magic, version)


def write_delimited_to(msg, output):
    """Write delimited message."""
    write32(msg.ByteSize(), output)
    output.write(msg.SerializeToString())


def read_delimited_from(_input, msg) -> bool:
    """Read delimited message."""
    if size := read32(_input):
        msg.ParseFromString(_input.read(size))
        return True
    return False
