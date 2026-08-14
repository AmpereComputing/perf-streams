# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

"""Protobuf read/write utilities."""

import struct
from typing import Protocol, cast

INTEGER_SIZE = 4
SIZE_STRUCT = struct.Struct("<L")


class Reader(Protocol):
    """Readable binary stream."""

    def read(self, size: int = -1) -> bytes:
        """Read up to `size` bytes from the stream."""
        ...


class Writer(Protocol):
    """Writable binary stream."""

    def write(self, data: bytes) -> int:
        """Write bytes to the stream."""
        ...


class Message(Protocol):
    """Subset of the protobuf message API used by this module."""

    def ByteSize(self) -> int:  # noqa: N802
        """Return the serialized message size."""

    def SerializeToString(self) -> bytes:  # noqa: N802
        """Serialize the message to bytes."""

    def ParseFromString(self, serialized: bytes) -> int:  # noqa: N802
        """Parse a serialized message."""


def is32(num_bytes: bytes) -> bool:
    """Check if bytes can be an integer."""
    return len(num_bytes) == INTEGER_SIZE


def read32(_input: Reader) -> int | None:
    """Get message size in bytes, encoded as bytes."""
    num_bytes = _input.read(INTEGER_SIZE)
    if not is32(num_bytes):
        return None
    return cast("int", SIZE_STRUCT.unpack(num_bytes)[0])


def write32(value: int, output: Writer) -> None:
    """Write message size in bytes, encoded as bytes."""
    output.write(SIZE_STRUCT.pack(value))


def write_header_to(magic: int, version: int, output: Writer) -> None:
    """Write header (magic + version)."""
    write32(magic, output)
    write32(version, output)


def read_header_from(_input: Reader) -> tuple[int | None, int | None]:
    """Read header (magic + version)."""
    magic = read32(_input)
    version = read32(_input)
    return (magic, version)


def write_delimited_to(msg: Message, output: Writer) -> None:
    """Write delimited message."""
    write32(msg.ByteSize(), output)
    output.write(msg.SerializeToString())


def read_delimited_from(_input: Reader, msg: Message) -> bool:
    """Read delimited message."""
    if (size := read32(_input)) is not None:
        msg.ParseFromString(_input.read(size))
        return True
    return False
