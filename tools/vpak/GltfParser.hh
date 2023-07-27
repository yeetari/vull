#pragma once

#include <vull/container/FixedBuffer.hh>
#include <vull/json/Parser.hh> // TODO: Only need ParseError.
#include <vull/json/Tree.hh>   // TODO: Only need JsonError.
#include <vull/platform/FileStream.hh>
#include <vull/support/String.hh>
#include <vull/vpak/Writer.hh>

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
