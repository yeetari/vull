#pragma once

#include <vull/container/fixed_buffer.hh>
#include <vull/json/parser.hh> // TODO: Only need ParseError.
#include <vull/json/tree.hh>   // TODO: Only need JsonError.
#include <vull/platform/file_stream.hh>
#include <vull/support/string.hh>
#include <vull/vpak/writer.hh>

namespace vull {

class GltfParser {
    FileStream m_stream;
    String m_json;
    ByteBuffer m_binary_blob;

public:
    explicit GltfParser(FileStream &&stream) : m_stream(vull::move(stream)) {}

    // TODO: Split errors.
    enum class Error {
        BadBinaryChunk,
        BadJsonChunk,
        InvalidMagic,
        SizeMismatch,
        UnsupportedVersion,

        BadVectorArrayLength,
        OffsetOutOfBounds,
        UnsupportedImageMimeType,
        UnsupportedNodeMatrix,
        UnsupportedNormalisedAccessor,
        UnsupportedPrimitiveMode,
        UnsupportedSparseAccessor,
    };
    Result<void, Error, StreamError> parse_glb();
    Result<void, Error, StreamError, json::ParseError, json::TreeError> convert(vpak::Writer &pack_writer,
                                                                                bool max_resolution, bool reproducible);

    const String &json() const { return m_json; }
};

} // namespace vull
