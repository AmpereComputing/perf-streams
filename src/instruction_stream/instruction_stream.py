# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

"""Event Stream reader/writer utilities."""

import perf_streams.instruction_stream_pb2 as is_proto
from perf_streams.protobuf_stream import ProtobufStreamReader, ProtobufStreamWriter

protobuf_is_magic = 0x53494250  # 0x50(P) 0x42(B) 0x49(I) 0x53(S)
protobuf_is_version = 6  # synchronize with instruction_stream.cc


class InstructionStreamWriter(ProtobufStreamWriter):
    """Protobuf-based InstructionStream writer."""

    def __init__(self, filename: str, features: is_proto.Features) -> None:
        """Initialize an instruction stream for writing, including writing the passed-in features."""
        super().__init__(filename, protobuf_is_magic, protobuf_is_version)
        self.write(features)

    def write_instruction(self, instruction: is_proto.Instruction) -> None:
        """Write instruction to stream."""
        event = is_proto.Event()
        event.instruction.CopyFrom(instruction)
        super().write(event)

    def write(self, msg: is_proto.Event | is_proto.Instruction | is_proto.Features) -> None:
        """Write message to stream."""
        if isinstance(msg, is_proto.Event):
            super().write(msg)
        elif isinstance(msg, is_proto.Instruction):
            self.write_instruction(msg)
        elif isinstance(msg, is_proto.Features):
            super().write(msg)
        else:
            raise Exception(f"InstructionStreamWriter.write called with unexpected message type: {type(msg).__name__}")


class InstructionStreamReader(ProtobufStreamReader):
    """Protobuf-based InstructionStream reader."""

    def __init__(self, filename: str):
        """Initialize an instruction stream for reading, including reading the on-disk features."""
        super().__init__(filename, protobuf_is_magic, protobuf_is_version)
        self.features = is_proto.Features()
        self.read(self.features)

    def read_instruction(self, instruction: is_proto.Instruction) -> bool:
        """Read instruction from stream."""
        event = is_proto.Event()
        status = super().read(event)
        while status and not event.HasField("instruction"):
            status = super().read(event)
        if status:
            instruction.CopyFrom(event.instruction)
        return status

    def read(self, msg: is_proto.Event | is_proto.Instruction | is_proto.Features) -> bool:
        """Read message from stream."""
        if isinstance(msg, is_proto.Event):
            return super().read(msg)
        if isinstance(msg, is_proto.Instruction):
            return self.read_instruction(msg)
        if isinstance(msg, is_proto.Features):
            return super().read(msg)
        raise Exception(f"InstructionStreamReader.read called with unexpected message type: {type(msg).__name__}")
