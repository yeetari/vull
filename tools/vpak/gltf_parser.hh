#pragma once

#include <vull/container/fixed_buffer.hh>
#include <vull/json/parser.hh> // TODO: Only need ParseError.
#include <vull/json/tree.hh>   // TODO: Only need JsonError.
#include <vull/platform/file_stream.hh>
#include <vull/support/result.hh>
#include <vull/support/stream.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>

namespace vull::vpak {

class Writer;

} // namespace vull::vpak

namespace vull {

enum class PngError;

enum class GlbError {
    BadBinaryChunk,
    BadJsonChunk,
    InvalidMagic,
    SizeMismatch,
    UnsupportedVersion,
};

enum class GltfError {
    BadVectorArrayLength,
    OffsetOutOfBounds,
    UnsupportedImageMimeType,
    UnsupportedNodeMatrix,
    UnsupportedNormalisedAccessor,
    UnsupportedPrimitiveMode,
    UnsupportedSparseAccessor,
};

template <typename T = void>
using GltfResult = Result<T, GltfError, StreamError, json::ParseError, json::TreeError, PngError>;

class GltfParser {
    platform::FileStream m_stream;
    String m_json;
    ByteBuffer m_binary_blob;

public:
    explicit GltfParser(platform::FileStream &&stream) : m_stream(vull::move(stream)) {}

    Result<void, GlbError, StreamError> parse_glb();
    GltfResult<> convert(vpak::Writer &pack_writer, bool max_resolution, bool reproducible);

    const String &json() const { return m_json; }
};

} // namespace vull
