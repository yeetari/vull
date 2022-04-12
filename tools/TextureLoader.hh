#pragma once

namespace vull {

class PackWriter;
class String;

} // namespace vull

bool load_texture(vull::PackWriter &pack_writer, const vull::String &path);
