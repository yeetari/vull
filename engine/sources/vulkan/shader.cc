#include <vull/vulkan/shader.hh>

#include <vull/container/array.hh>
#include <vull/container/fixed_buffer.hh>
#include <vull/container/vector.hh>
#include <vull/support/algorithm.hh>
#include <vull/support/assert.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/vpak/file_system.hh>
#include <vull/vpak/pack_file.hh>
#include <vull/vpak/reader.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/spirv.hh>
#include <vull/vulkan/vulkan.hh>

namespace vull {

enum class StreamError;

} // namespace vull

namespace vull::vk {
namespace {

enum class Endian {
    Little,
    Big,
};

Endian determine_endian(Span<const uint8_t> magic) {
    for (uint32_t i = 0; i < 4; i++) {
        if (magic[i] != spv::k_magic_bytes[i]) {
            return Endian::Little;
        }
    }
    return Endian::Big;
}

vkb::ShaderStage to_shader_stage(spv::Word word) {
    switch (static_cast<spv::ExecutionModel>(word)) {
    case spv::ExecutionModel::Vertex:
        return vkb::ShaderStage::Vertex;
    case spv::ExecutionModel::Fragment:
        return vkb::ShaderStage::Fragment;
    case spv::ExecutionModel::GLCompute:
        return vkb::ShaderStage::Compute;
    default:
        return vkb::ShaderStage::All;
    }
}

struct IdInfo {
    String name;
    spv::Op opcode;
    spv::StorageClass storage_class;
    spv::Decoration main_decoration;
    spv::Id type_id;
    union {
        uint8_t bit_width;       // For OpTypeBool, OpTypeInt, OpTypeFloat
        uint8_t component_count; // For OpTypeVector
        uint8_t location;        // For Input/Output storage classes
        uint8_t spec_id;         // For OpSpecConstant[True, False]
    };
    bool is_signed; // For OpTypeInt
};

// TODO: Can be generated.
uint32_t format_size(vkb::Format format) {
    switch (format) {
    case vkb::Format::R32Uint:
    case vkb::Format::R32Sint:
    case vkb::Format::R32Sfloat:
        return 4;
    case vkb::Format::R32G32Uint:
    case vkb::Format::R32G32Sint:
    case vkb::Format::R32G32Sfloat:
        return 8;
    case vkb::Format::R32G32B32Uint:
    case vkb::Format::R32G32B32Sint:
    case vkb::Format::R32G32B32Sfloat:
        return 12;
    case vkb::Format::R32G32B32A32Uint:
    case vkb::Format::R32G32B32A32Sint:
    case vkb::Format::R32G32B32A32Sfloat:
        return 16;
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

} // namespace

void Shader::add_entry_point(String name, vkb::ShaderStage stage, Vector<spv::Id> &&interface_ids) {
    m_entry_points.push({vull::move(name), stage, vull::move(interface_ids)});
}

void Shader::add_constant(spv::Id id, String name, uint32_t size) {
    m_constants.ensure_size(id + 1);
    if (m_constants[id].name.empty()) {
        m_constants[id] = {
            .name = vull::move(name),
            .id = id,
            .size = size,
        };
    }
}

void Shader::add_vertex_attribute(uint32_t location, vkb::Format format) {
    m_vertex_attributes.push({
        .location = location,
        .format = format,
    });
}

Result<Shader, ShaderError> Shader::parse(const Context &context, Span<const uint8_t> data) {
    if (data.size() % sizeof(spv::Word) != 0) {
        return ShaderError::BadSize;
    }
    const auto total_word_count = static_cast<uint32_t>(data.size() / sizeof(spv::Word));
    if (total_word_count < 5) {
        return ShaderError::BadSize;
    }

    const auto endian = determine_endian(data);
    Vector<spv::Word> words(total_word_count);
    for (uint32_t i = 0; i < total_word_count; i++) {
        spv::Word &word = words[i];
        if (endian == Endian::Big) {
            word |= static_cast<spv::Word>(data[i * sizeof(spv::Word) + 0] << 24u);
            word |= static_cast<spv::Word>(data[i * sizeof(spv::Word) + 1] << 16u);
            word |= static_cast<spv::Word>(data[i * sizeof(spv::Word) + 2] << 8u);
            word |= static_cast<spv::Word>(data[i * sizeof(spv::Word) + 3] << 0u);
        } else {
            word |= static_cast<spv::Word>(data[i * sizeof(spv::Word) + 0] << 0u);
            word |= static_cast<spv::Word>(data[i * sizeof(spv::Word) + 1] << 8u);
            word |= static_cast<spv::Word>(data[i * sizeof(spv::Word) + 2] << 16u);
            word |= static_cast<spv::Word>(data[i * sizeof(spv::Word) + 3] << 24u);
        }
    }

    if (words[0] != spv::k_magic_number) {
        return ShaderError::BadMagic;
    }
    if (words[1] < 0x10400) {
        // SPIR-V 1.4 changed the semantics of OpEntryPoint.
        return ShaderError::BadVersion;
    }
    const auto id_bound = words[3];
    if (id_bound > 10000) {
        return ShaderError::BadSize;
    }

    Vector<IdInfo> id_infos(id_bound);
    auto id_info = [&](spv::Id id) -> Result<IdInfo &, ShaderError> {
        if (id >= id_bound) {
            return ShaderError::Malformed;
        }
        return id_infos[id];
    };
    auto type_format = [&](spv::Id id) -> Result<vkb::Format, ShaderError> {
        const IdInfo &info = VULL_TRY(id_info(id));
        if (info.opcode == spv::Op::TypeFloat && info.bit_width == 32) {
            return vkb::Format::R32Sfloat;
        }
        if (info.opcode == spv::Op::TypeInt && info.bit_width == 32) {
            return info.is_signed ? vkb::Format::R32Sint : vkb::Format::R32Uint;
        }
        if (info.opcode != spv::Op::TypeVector) {
            return ShaderError::Unhandled;
        }
        const IdInfo &comp_info = VULL_TRY(id_info(info.type_id));
        if (comp_info.opcode == spv::Op::TypeFloat && comp_info.bit_width == 32) {
            switch (info.component_count) {
            case 2:
                return vkb::Format::R32G32Sfloat;
            case 3:
                return vkb::Format::R32G32B32Sfloat;
            case 4:
                return vkb::Format::R32G32B32A32Sfloat;
            }
        }
        if (comp_info.opcode == spv::Op::TypeInt && comp_info.bit_width == 32) {
            switch (info.component_count) {
            case 2:
                return comp_info.is_signed ? vkb::Format::R32G32Sint : vkb::Format::R32G32Uint;
            case 3:
                return comp_info.is_signed ? vkb::Format::R32G32B32Sint : vkb::Format::R32G32B32Uint;
            case 4:
                return comp_info.is_signed ? vkb::Format::R32G32B32A32Sint : vkb::Format::R32G32B32A32Uint;
            }
        }
        return ShaderError::Unhandled;
    };

    Shader shader(context);
    for (uint32_t ip = 5; ip < words.size();) {
        const uint16_t opcode = (words[ip] >> 0u) & 0xffffu;
        const uint16_t word_count = (words[ip] >> 16u) & 0xffffu;
        if (ip + word_count > words.size()) {
            return ShaderError::Malformed;
        }

        const auto inst_words = words.span().subspan(ip, word_count);
        switch (static_cast<spv::Op>(opcode)) {
        case spv::Op::Name: {
            if (word_count < 3) {
                return ShaderError::Malformed;
            }

            StringBuilder name_builder;
            for (uint32_t offset = 2; offset < word_count; offset++) {
                spv::Word word = inst_words[offset];
                for (uint32_t i = 0; i < 3; i++) {
                    if (word != 0) {
                        name_builder.append(static_cast<char>(word & 0xffu));
                        word >>= 8u;
                    }
                }
                if (word == 0) {
                    break;
                }
                name_builder.append(static_cast<char>(word & 0xffu));
            }
            VULL_TRY(id_info(inst_words[1])).name = name_builder.build();
            break;
        }
        case spv::Op::EntryPoint: {
            if (word_count < 4) {
                return ShaderError::Malformed;
            }

            uint32_t offset = 3;
            StringBuilder name_builder;
            for (; offset < word_count; offset++) {
                spv::Word word = inst_words[offset];
                for (uint32_t i = 0; i < 3; i++) {
                    if (word != 0) {
                        name_builder.append(static_cast<char>(word & 0xffu));
                        word >>= 8u;
                    }
                }
                if (word == 0) {
                    break;
                }
                name_builder.append(static_cast<char>(word & 0xffu));
            }
            offset++;

            // Now we have the interface IDs.
            Vector<spv::Id> interface_ids;
            interface_ids.extend(inst_words.subspan(offset));
            shader.add_entry_point(name_builder.build(), to_shader_stage(inst_words[1]), vull::move(interface_ids));
            break;
        }
        case spv::Op::SpecConstantTrue:
        case spv::Op::SpecConstantFalse:
        case spv::Op::SpecConstant: {
            if (word_count < 3) {
                return ShaderError::Malformed;
            }
            IdInfo &info = VULL_TRY(id_info(inst_words[2]));
            info.type_id = inst_words[1];

            IdInfo &type_info = VULL_TRY(id_info(info.type_id));
            VULL_ENSURE(type_info.opcode == spv::Op::TypeBool || type_info.opcode == spv::Op::TypeInt ||
                        type_info.opcode == spv::Op::TypeFloat);
            shader.add_constant(info.spec_id, info.name, type_info.bit_width / 8);
            break;
        }
        case spv::Op::Decorate: {
            if (word_count < 3) {
                return ShaderError::Malformed;
            }
            const auto decoration = static_cast<spv::Decoration>(inst_words[2]);
            IdInfo &info = VULL_TRY(id_info(inst_words[1]));
            switch (decoration) {
            case spv::Decoration::BuiltIn:
            case spv::Decoration::Location:
                info.main_decoration = decoration;
                break;
            }
            if (decoration == spv::Decoration::Location) {
                info.location = static_cast<uint8_t>(inst_words[3]);
            }
            if (decoration == spv::Decoration::SpecId) {
                info.spec_id = static_cast<uint8_t>(inst_words[3]);
            }
            break;
        }
        case spv::Op::Variable: {
            if (word_count < 4) {
                return ShaderError::Malformed;
            }
            IdInfo &info = VULL_TRY(id_info(inst_words[2]));
            info.storage_class = static_cast<spv::StorageClass>(inst_words[3]);
            info.type_id = inst_words[1];
            break;
        }
        case spv::Op::TypeBool: {
            if (word_count != 2) {
                return ShaderError::Malformed;
            }
            IdInfo &info = VULL_TRY(id_info(inst_words[1]));
            info.opcode = spv::Op::TypeBool;
            info.bit_width = 32;
            break;
        }
        case spv::Op::TypeInt: {
            if (word_count != 4) {
                return ShaderError::Malformed;
            }
            IdInfo &info = VULL_TRY(id_info(inst_words[1]));
            info.opcode = spv::Op::TypeInt;
            info.bit_width = static_cast<uint8_t>(inst_words[2]);
            info.is_signed = inst_words[3] == 1;
            break;
        }
        case spv::Op::TypeFloat: {
            if (word_count != 3) {
                return ShaderError::Malformed;
            }
            IdInfo &info = VULL_TRY(id_info(inst_words[1]));
            info.opcode = spv::Op::TypeFloat;
            info.bit_width = static_cast<uint8_t>(inst_words[2]);
            break;
        }
        case spv::Op::TypeVector: {
            if (word_count != 4) {
                return ShaderError::Malformed;
            }
            IdInfo &info = VULL_TRY(id_info(inst_words[1]));
            info.opcode = spv::Op::TypeVector;
            info.type_id = inst_words[2];
            info.component_count = static_cast<uint8_t>(inst_words[3]);
            break;
        }
        case spv::Op::TypePointer: {
            if (word_count != 4) {
                return ShaderError::Malformed;
            }
            IdInfo &info = VULL_TRY(id_info(inst_words[1]));
            info.opcode = spv::Op::TypePointer;
            info.type_id = inst_words[3];
            break;
        }
        }
        ip += word_count;
    }

    if (shader.entry_points().empty()) {
        return ShaderError::NoEntryPoint;
    }

    for (const auto &[name, stage, interface_ids] : shader.entry_points()) {
        if (stage != vkb::ShaderStage::Vertex) {
            continue;
        }
        for (spv::Id id : interface_ids) {
            const IdInfo &info = VULL_TRY(id_info(id));
            if (info.storage_class == spv::StorageClass::Input && info.main_decoration == spv::Decoration::Location) {
                const IdInfo &pointer_type = VULL_TRY(id_info(info.type_id));
                if (pointer_type.opcode != spv::Op::TypePointer) {
                    return ShaderError::Malformed;
                }
                shader.add_vertex_attribute(info.location, VULL_TRY(type_format(pointer_type.type_id)));
            }
        }
    }

    vull::sort(shader.vertex_attributes(), [](const auto &lhs, const auto &rhs) {
        return lhs.location > rhs.location;
    });
    uint32_t vertex_stride = 0;
    for (auto &attribute : shader.vertex_attributes()) {
        attribute.offset = vertex_stride;
        vertex_stride += format_size(attribute.format);
    }
    shader.set_vertex_stride(vertex_stride);

    vkb::ShaderModuleCreateInfo module_ci{
        .sType = vkb::StructureType::ShaderModuleCreateInfo,
        .codeSize = words.size_bytes(),
        .pCode = words.data(),
    };
    vkb::ShaderModule module;
    if (context.vkCreateShaderModule(&module_ci, &module) != vkb::Result::Success) {
        return ShaderError::ModuleCreation;
    }
    shader.set_module(module);
    return vull::move(shader);
}

Result<Shader, ShaderError, StreamError> Shader::load(const Context &context, StringView name) {
    auto entry = vpak::stat(name);
    auto stream = vpak::open(name);
    VULL_ENSURE(entry && stream);
    auto binary = FixedBuffer<uint8_t>::create_uninitialised(entry->size);
    VULL_TRY(stream->read(binary.span()));
    return VULL_TRY(parse(context, binary.span()));
}

Shader::Shader(Shader &&other) {
    m_context = vull::exchange(other.m_context, nullptr);
    m_module = vull::exchange(other.m_module, nullptr);
    m_entry_points = vull::move(other.m_entry_points);
    m_constants = vull::move(other.m_constants);
    m_vertex_attributes = vull::move(other.m_vertex_attributes);
    m_vertex_stride = vull::exchange(other.m_vertex_stride, 0u);
}

Shader::~Shader() {
    if (m_context != nullptr) {
        m_context->vkDestroyShaderModule(m_module);
    }
}

Shader &Shader::operator=(Shader &&other) {
    Shader moved(vull::move(other));
    vull::swap(m_context, moved.m_context);
    vull::swap(m_module, moved.m_module);
    vull::swap(m_entry_points, moved.m_entry_points);
    vull::swap(m_constants, moved.m_constants);
    vull::swap(m_vertex_attributes, moved.m_vertex_attributes);
    vull::swap(m_vertex_stride, moved.m_vertex_stride);
    return *this;
}

} // namespace vull::vk
