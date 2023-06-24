#include "MadInst.hh"

#include <vull/container/Vector.hh>
#include <vull/core/Log.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Vec.hh>
#include <vull/platform/File.hh>
#include <vull/platform/FileStream.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Enum.hh>
#include <vull/support/Format.hh>
#include <vull/support/Result.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>

#include <stddef.h>
#include <stdint.h>

using namespace vull;

int main() {
    auto binary = VULL_EXPECT(vull::open_file("mad_lut.bin", OpenMode::Create | OpenMode::Truncate | OpenMode::Write));
    auto binary_stream = binary.create_stream();
    auto source = VULL_EXPECT(
        vull::open_file("../tools/vpak/MadLut.in", OpenMode::Create | OpenMode::Truncate | OpenMode::Write));
    auto source_stream = source.create_stream();

    size_t offset = 0;
    auto build_filter = [&](StringView name, Filter filter) {
        VULL_EXPECT(source_stream.write_c_string(vull::format("Array {}{\n", name)));
        for (uint32_t log_x = 0; log_x < 13; log_x++) {
            const auto source_x = 1u << log_x;
            VULL_EXPECT(source_stream.write_c_string("Array{\n"));
            for (uint32_t log_y = 0; log_y < 13; log_y++) {
                const auto source_y = 1u << log_y;
                VULL_EXPECT(source_stream.write_c_string("Array{\n"));

                for (uint32_t log_width = 0; log_width < 12; log_width++) {
                    if (log_width > vull::min(log_x, log_y)) {
                        VULL_EXPECT(source_stream.write_c_string("-1,\n"));
                        continue;
                    }
                    VULL_EXPECT(source_stream.write_c_string(vull::format("{},\n", offset)));
                    const auto target_width = 1u << log_width;
                    vull::info("[build-lut] Building {}x{} -> {} ({})", source_x, source_y, target_width,
                               vull::enum_name(filter));
                    auto mad = build_mad_program(Vec2u(source_x, source_y), target_width, filter);
                    VULL_EXPECT(binary_stream.write_be(mad.size()));
                    for (const auto &inst : mad) {
                        VULL_EXPECT(binary_stream.write_be(inst.target_index));
                        VULL_EXPECT(binary_stream.write_be(inst.source_index));
                        VULL_EXPECT(binary_stream.write({&inst.weight, sizeof(float)}));
                    }
                    offset += mad.size_bytes() + sizeof(uint32_t);
                }
                VULL_EXPECT(source_stream.write_c_string("},\n"));
            }
            VULL_EXPECT(source_stream.write_c_string("},\n"));
        }
        VULL_EXPECT(source_stream.write_c_string("};\n"));
    };
    build_filter("s_box_offset_lut", Filter::Box);
    build_filter("s_gaussian_offset_lut", Filter::Gaussian);
}
