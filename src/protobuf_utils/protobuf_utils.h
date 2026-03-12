/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <google/protobuf/io/coded_stream.h>     // IWYU pragma: export
#include <google/protobuf/io/zero_copy_stream.h> // IWYU pragma: export
#include <google/protobuf/message_lite.h>        // IWYU pragma: export
#include <memory>

namespace perf_streams::protobuf_utils {

bool write_header_to(uint32_t magic, uint32_t version, google::protobuf::io::ZeroCopyOutputStream* raw_output);
template<typename T>
inline bool write_header_to(uint32_t magic, uint32_t version, const std::unique_ptr<T>& raw_output)
{
    return write_header_to(magic, version, raw_output.get());
}

bool read_header_from(google::protobuf::io::ZeroCopyInputStream* raw_input, uint32_t* magic, uint32_t* version);
template<typename T>
inline bool read_header_from(const std::unique_ptr<T>& raw_input, uint32_t* magic, uint32_t* version)
{
    return read_header_from(raw_input.get(), magic, version);
}

bool write_delimited_to(const google::protobuf::MessageLite& message,
                        google::protobuf::io::ZeroCopyOutputStream* raw_output);
template<typename T>
inline bool write_delimited_to(const google::protobuf::MessageLite& message, const std::unique_ptr<T>& raw_output)
{
    return write_delimited_to(message, raw_output.get());
}

bool read_delimited_from(google::protobuf::io::ZeroCopyInputStream* raw_input,
                         google::protobuf::MessageLite* message,
                         bool* clean_eof);
template<typename T>
inline bool read_delimited_from(const std::unique_ptr<T>& raw_input,
                                google::protobuf::MessageLite* message,
                                bool* clean_eof)
{
    return read_delimited_from(raw_input.get(), message, clean_eof);
}

inline bool clear_and_read_delimited_from(google::protobuf::io::ZeroCopyInputStream* raw_input,
                                          google::protobuf::MessageLite* message,
                                          bool* clean_eof)
{
    message->Clear();
    return read_delimited_from(raw_input, message, clean_eof);
}
template<typename T>
inline bool clear_and_read_delimited_from(const std::unique_ptr<T>& raw_input,
                                          google::protobuf::MessageLite* message,
                                          bool* clean_eof)
{
    return clear_and_read_delimited_from(raw_input.get(), message, clean_eof);
}

class DelimitedReader
{
    uint32_t size;
    google::protobuf::io::CodedInputStream input;

    bool read_size() { return input.ReadLittleEndian32(&size); }
    bool read_message(google::protobuf::MessageLite* message)
    {
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

public:
    explicit DelimitedReader(google::protobuf::io::ZeroCopyInputStream* raw_input) : input(raw_input) {}
    template<typename T>
    explicit DelimitedReader(const std::unique_ptr<T>& raw_input) : DelimitedReader(raw_input.get())
    {
    }

    bool read_header(uint32_t* magic, uint32_t* version)
    {
        return input.ReadLittleEndian32(magic) && input.ReadLittleEndian32(version);
    }

    bool read(google::protobuf::MessageLite* message)
    {
        if (!read_size())
            return false;
        return read_message(message);
    }

    bool read(google::protobuf::MessageLite* message, bool* clean_eof)
    {
        const int start = input.CurrentPosition();
        if (clean_eof)
            *clean_eof = false;

        if (!read_size()) {
            if (clean_eof)
                *clean_eof = input.CurrentPosition() == start;
            return false;
        }

        return read_message(message);
    }
};

} // namespace perf_streams::protobuf_utils
