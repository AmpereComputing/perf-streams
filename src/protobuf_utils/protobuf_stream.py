# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

"""Protobuf stream read/write utilities."""

import subprocess
from contextlib import suppress
from io import DEFAULT_BUFFER_SIZE, IOBase
from os import cpu_count, path

try:
    import lzma
except ImportError:
    from backports import lzma

from google.protobuf.message import Message
from perf_streams.protobuf_utils import SIZE_STRUCT, is32, read_header_from, write_delimited_to, write_header_to


class ProtobufStreamReader:
    """Protobuf stream (delimited) reader."""

    def __init__(self, filename: str | IOBase, expected_magic: int, max_version: int) -> None:
        """Initialize protobuf stream reader at file, checking magic/version."""
        self._file_process = None
        if isinstance(filename, IOBase):
            self.file = filename
            self.close_when_done = False
        elif filename.endswith(".xz"):
            if not path.exists(filename):
                raise FileNotFoundError(filename)
            try:
                # Attempt using xz as lzma does not currently support threading
                threads = min(4, cpu_count() or 1)
                self._file_process = subprocess.Popen(
                    ["xz", "-T", str(threads), "-d", "-c", filename],
                    stderr=subprocess.PIPE,
                    stdout=subprocess.PIPE,
                )
                self.file = self._file_process.stdout
                if self.file is None:
                    raise RuntimeError("xz decompressor did not provide stdout")
            except FileNotFoundError:
                self._file_process = None
                self.file = lzma.open(filename, "rb")
            self.close_when_done = True
        else:
            self.file = open(filename, "rb")  # noqa: SIM115
            self.close_when_done = True

        magic, version = read_header_from(self.file)
        if magic is None or version is None:
            self._check_decompressor_status(block=True)
        self.version = version
        assert expected_magic == magic, (
            f"ProtobufStreamReader expected magic value to be {expected_magic}, but found {magic} instead"
        )
        assert self.version <= max_version, (
            f"ProtobufStreamReader expected version to be <={max_version}, but found {self.version} instead"
        )

        self.read_buffer_size = DEFAULT_BUFFER_SIZE
        self.read_buffer = bytearray(self.read_buffer_size)
        self.read_view = memoryview(self.read_buffer)
        self.read_index = 0
        self.write_index = 0

    def _check_decompressor_status(self, block: bool = False) -> None:
        """Raise if the external decompressor has failed."""
        if self._file_process is None:
            return

        returncode = None if block else self._file_process.poll()
        if returncode is None:
            if not block:
                return
            _, stderr_bytes = self._file_process.communicate()
            returncode = self._file_process.returncode
        else:
            _, stderr_bytes = self._file_process.communicate()

        stderr = stderr_bytes.decode(errors="replace").strip() if stderr_bytes else ""

        self._file_process = None
        if returncode != 0:
            message = f"xz decompression failed with status {returncode}"
            if stderr:
                message += f": {stderr}"
            raise RuntimeError(message)

    def _read_file(self, bytes_to_read: int) -> bytes | bytearray | memoryview:
        """
        Handle reading from the file, but do the underlying read in
        buffer-sized chunks to increase the efficiency.
        """
        # Ensure that one buffer's worth is large enough to fit the item we are
        # reading. This avoids having to stitch together more than two buffers.
        if bytes_to_read > self.read_buffer_size:
            while bytes_to_read > self.read_buffer_size:
                self.read_buffer_size *= 2

            self.read_buffer = bytearray(self.read_buffer_size)
            self.read_buffer[self.read_index : self.write_index] = self.read_view[self.read_index : self.write_index]
            self.read_view.release()  # release the old bytearray from the view
            self.read_view = memoryview(self.read_buffer)

        if self.read_index + bytes_to_read >= self.write_index:
            # We're going to have to try to read another buffer's worth and
            # concatenate the results across buffers.
            remainder = bytearray(self.read_view[self.read_index : self.write_index])

            data = self.file.read(self.read_buffer_size)
            self.write_index = len(data)
            self.read_view[0 : self.write_index] = data
            self.read_index = min(bytes_to_read - len(remainder), self.write_index)

            return remainder + self.read_view[0 : self.read_index]

        # We can simply return a memoryview into the existing buffer.
        old_read_idx = self.read_index
        self.read_index += bytes_to_read
        return self.read_view[old_read_idx : self.read_index]

    def read(self, item: Message) -> bool:
        """Read item from stream."""
        # Get message size in bytes, encoded as bytes
        num_bytes = self._read_file(4)
        if not is32(num_bytes):
            self._check_decompressor_status(block=True)
            return False
        size = SIZE_STRUCT.unpack(num_bytes)[0]
        # Read the message from the file and decode it. Note that
        # ParseFromString handles clearing the protobuf 'message' object (named
        # 'item' here) so callers of this method don't need to.
        item_bytes = self._read_file(size)
        if len(item_bytes) != size:
            self._check_decompressor_status(block=True)
        item.ParseFromString(bytes(item_bytes))
        return True

    def close(self) -> None:
        """Close stream."""
        if hasattr(self, "file") and self.close_when_done:
            try:
                self._check_decompressor_status()
            finally:
                self.file.close()
                del self.file
            if self._file_process is not None:
                self._file_process.wait()
                if self._file_process.stderr is not None:
                    self._file_process.stderr.close()
                self._file_process = None

    def __del__(self) -> None:
        """Close stream."""
        with suppress(Exception):
            self.close()


class ProtobufStreamWriter:
    """Protobuf stream (delimited) writer."""

    def __init__(self, filename: str, magic: int, version: int) -> None:
        """Initialize protobuf stream writer at file with magic/version."""
        if filename.endswith(".xz"):
            self.file = lzma.open(filename, "wb")
        else:
            self.file = open(filename, "wb")  # noqa: SIM115

        self.version = version
        write_header_to(magic, version, self.file)
        self.file.flush()

    def write(self, item: Message) -> None:
        """Write item to stream."""
        write_delimited_to(item, self.file)

    def close(self) -> None:
        """Close stream."""
        if hasattr(self, "file"):
            self.file.flush()
            self.file.close()
            del self.file

    def __del__(self) -> None:
        """Close stream."""
        self.close()
