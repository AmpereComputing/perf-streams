// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "protobuf_utils.h"

#include <cstdint>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/message_lite.h>

namespace perf_streams::protobuf_utils {

bool write_header_to(uint32_t magic, uint32_t version, google::protobuf::io::ZeroCopyOutputStream* raw_output)
{
    // We create a new coded stream for each message.  Don't worry, this is fast.
    google::protobuf::io::CodedOutputStream output(raw_output);

    output.WriteLittleEndian32(magic);
    output.WriteLittleEndian32(version);

    return !output.HadError();
}

bool read_header_from(google::protobuf::io::ZeroCopyInputStream* raw_input, uint32_t* magic, uint32_t* version)
{
    // We create a new coded stream for each message.  Don't worry, this is fast,
    // and it makes sure the 64MB total size limit is imposed per-message rather
    // than on the whole stream.  (See the CodedInputStream interface for more
    // info on this limit.)
    google::protobuf::io::CodedInputStream input(raw_input);

    return input.ReadLittleEndian32(magic) && input.ReadLittleEndian32(version);
}

bool write_delimited_to(const google::protobuf::MessageLite& message,
                        google::protobuf::io::ZeroCopyOutputStream* raw_output)
{
    // We create a new coded stream for each message.  Don't worry, this is fast.
    google::protobuf::io::CodedOutputStream output(raw_output);

    // Write the size.
    const int size = message.ByteSizeLong();
    output.WriteLittleEndian32(size);

    uint8_t* buffer = output.GetDirectBufferForNBytesAndAdvance(size);
    if (buffer != nullptr) {
        // Optimization:  The message fits in one buffer, so use the faster
        // direct-to-array serialization path.
        uint8_t* buffer_end = message.SerializeWithCachedSizesToArray(buffer);
        return buffer_end == buffer + size;
    } else {
        // Slightly-slower path when the message is multiple buffers.
        message.SerializeWithCachedSizes(&output);
        return !output.HadError();
    }
}

bool read_delimited_from(google::protobuf::io::ZeroCopyInputStream* raw_input,
                         google::protobuf::MessageLite* message,
                         bool* clean_eof)
{
    // We create a new coded stream for each message.  Don't worry, this is fast,
    // and it makes sure the 64MB total size limit is imposed per-message rather
    // than on the whole stream.  (See the CodedInputStream interface for more
    // info on this limit.)
    google::protobuf::io::CodedInputStream input(raw_input);
    const int start = input.CurrentPosition();
    if (clean_eof)
        *clean_eof = false;

    // Read the size.
    uint32_t size;
    if (!input.ReadLittleEndian32(&size)) {
        if (clean_eof)
            *clean_eof = input.CurrentPosition() == start;
        return false;
    }
    // Tell the stream not to read beyond that size.
    auto limit = input.PushLimit(size);

    // Parse the message.
    if (!message->MergeFromCodedStream(&input))
        return false;
    if (!input.ConsumedEntireMessage())
        return false;

    // Release the limit.
    input.PopLimit(limit);

    return true;
}
} // namespace perf_streams::protobuf_utils
