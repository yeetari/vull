#pragma once

#include <vull/container/array.hh>

#include <stdint.h>

namespace vull::vk::spv {

using Word = uint32_t;
using Id = Word;

constexpr Word k_magic_number = 0x07230203;
constexpr Array<uint8_t, 4> k_magic_bytes{0x07, 0x23, 0x02, 0x03};

enum class AddressingModel : uint32_t {
    Logical = 0,
    PhysicalStorageBuffer64 = 5348,
};

enum class BuiltIn : uint32_t {
    Position = 0,
    PointSize = 1,
    ClipDistance = 3,
    CullDistance = 4,
    PrimitiveId = 7,
    InvocationId = 8,
    Layer = 9,
    ViewportIndex = 10,
    TessLevelOuter = 11,
    TessLevelInner = 12,
    TessCoord = 13,
    PatchVertices = 14,
    FragCoord = 15,
    PointCoord = 16,
    FrontFacing = 17,
    SampleMask = 20,
    FragDepth = 22,
    HelperInvocation = 23,
    NumWorkgroups = 24,
    // TODO: Deprecated?
    WorkgroupSize = 25,
    WorkgroupId = 26,
    LocalInvocationId = 27,
    GlobalInvocationId = 28,
    LocalInvocationIndex = 29,
    SubgroupSize = 36,
    NumSubgroups = 38,
    SubgroupId = 40,
    SubgroupLocalInvocationId = 41,
    VertexIndex = 42,
    InstanceIndex = 43,
    SubgroupEqMask = 4416,
    SubgroupGeMask = 4417,
    SubgroupGtMask = 4418,
    SubgroupLeMask = 4419,
    SubgroupLtMask = 4420,
    BaseVertex = 4424,
    BaseInstance = 4425,
    DrawIndex = 4426,
    ViewIndex = 4440,
};

enum class Capability : uint32_t {
    Matrix = 0,
    Shader = 1,
    Tessellation = 3,
    Float16 = 9,
    Float64 = 10,
    Int64 = 11,
    Int64Atomics = 12,
    AtomicStorage = 21,
    Int16 = 22,
    TessellationPointSize = 23,
    ImageGatherExtended = 25,
    StorageImageMultisample = 27,
    ClipDistance = 32,
    CullDistance = 33,
    ImageCubeArray = 34,
    Int8 = 39,
    InputAttachment = 40,
    SparseResidency = 41,
    MinLod = 42,
    SampledCubeArray = 45,
    SampledBuffer = 46,
    ImageBufer = 47,
    ImageMSArray = 48,
    StorageImageExtendedFormats = 49,
    ImageQuery = 50,
    DerivativeControl = 51,
    InterpolationFunction = 52,
    StorageImageReadWithoutFormat = 55,
    StorageImageWriteWithoutFormat = 56,
    GroupNonUniform = 61,
    GroupNonUniformVote = 62,
    GroupNonUniformArithmetic = 63,
    GroupNonUniformBallot = 64,
    GroupNonUniformShuffle = 65,
    GroupNonUniformShuffleRelative = 66,
    GroupNonUniformClustered = 67,
    GroupNonUniformQuad = 68,
    ShaderLayer = 69,
    ShaderViewportIndex = 70,
    DrawParameters = 4427,
    StorageBuffer16BitAccess = 4433,
    UniformAndStorageBuffer16BitAccess = 4434,
    StoragePushConstant16 = 4435,
    StorageInputOutput16 = 4436,
    MultiView = 4439,
    VariablePointersStorageBuffer = 4441,
    VariablePointers = 4442,
    StorageBuffer8BitAccess = 4448,
    UniformAndStorageBuffer8BitAccess = 4449,
    StoragePushConstant8 = 4450,
    ShaderNonUniform = 5301,
    RuntimeDescriptorArray = 5302,
    VulkanMemoryModel = 5345,
    PhysicalStorageBufferAddresses = 5347,
    DemoteToHelperInvocation = 5379,

};

enum class Decoration : uint32_t {
    RelaxedPrecision = 0,
    SpecId = 1,
    Block = 2,
    RowMajor = 4,
    ColMajor = 5,
    ArrayStride = 6,
    MatrixStride = 7,
    GlslShared = 8,
    GlslPacked = 9,
    BuiltIn = 11,
    NoPerspective = 13,
    Flat = 14,
    Patch = 15,
    Centroid = 16,
    Invariant = 18,
    Restrict = 19,
    Aliased = 20,
    Volatile = 21,
    Coherent = 23,
    NonWritable = 24,
    NonReadable = 25,
    Uniform = 26,
    UniformId = 27,
    Location = 30,
    Component = 31,
    Index = 32,
    Binding = 33,
    DescriptorSet = 34,
    Offset = 35,
    FpRoundingMode = 39,
    NoContraction = 42,
    InputAttachmentIndex = 43,
    NoSignedWrap = 4469,
    NoUnsignedWrap = 4470,
    NonUniform = 5300,
    RestrictPointer = 5355,
    AliasedPointer = 5356,
};

enum class ExecutionMode : uint32_t {
    SpacingEqual = 1,
    SpacingFractionalEven = 2,
    SpacingFractionalOdd = 3,
    VertexOrderCw = 4,
    VertexOrderCcw = 5,
    PixelCenterInteger = 6,
    OriginUpperLeft = 7,
    OriginLowerLeft = 8,
    EarlyFragmentTests = 9,
    PointMode = 10,
    DepthReplacing = 12,
    DepthGreater = 14,
    DepthLess = 15,
    DepthUnchanged = 16,
    LocalSize = 17,
    Triangles = 22,
    Quads = 24,
    Isolines = 25,
    OutputVertices = 26,
    LocalSizeId = 38,
};

enum class ExecutionModel : uint32_t {
    Vertex = 0,
    TesselationControl = 1,
    TesselationEvaluation = 2,
    Fragment = 4,
    GLCompute = 5,
};

enum class FunctionControl : uint32_t {
    None = 0,
    Inline = 1,
    NoInline = 2,
    Pure = 4,
    Const = 8,
};

enum class MemoryModel : uint32_t {
    Glsl450 = 1,
    Vulkan = 3,
};

enum class Op : uint32_t {
    Nop = 0,
    Name = 5,
    ExtInstImport = 11,
    ExtInst = 12,
    MemoryModel = 14,
    EntryPoint = 15,
    ExecutionMode = 16,
    Capability = 17,
    TypeVoid = 19,
    TypeBool = 20,
    TypeInt = 21,
    TypeFloat = 22,
    TypeVector = 23,
    TypeMatrix = 24,
    TypeStruct = 30,
    TypePointer = 32,
    TypeFunction = 33,
    Constant = 43,
    ConstantComposite = 44,
    SpecConstantTrue = 48,
    SpecConstantFalse = 49,
    SpecConstant = 50,
    Function = 54,
    FunctionEnd = 56,
    Variable = 59,
    Load = 61,
    Store = 62,
    AccessChain = 65,
    Decorate = 71,
    MemberDecorate = 72,
    CompositeConstruct = 80,
    CompositeExtract = 81,
    FNegate = 127,
    FAdd = 129,
    FSub = 131,
    FMul = 133,
    FDiv = 136,
    VectorTimesScalar = 142,
    MatrixTimesScalar = 143,
    VectorTimesMatrix = 144,
    MatrixTimesVector = 145,
    MatrixTimesMatrix = 146,
    Dot = 148,
    Label = 248,
    Branch = 249,
    BranchConditional = 250,
    Switch = 251,
    Return = 253,
    ReturnValue = 254,
    Unreachable = 255,
    TerminateInvocation = 4416,
};

enum class StorageClass : uint32_t {
    UniformConstant = 0,
    Input = 1,
    Uniform = 2,
    Output = 3,
    Workgroup = 4,
    CrossWorkgroup = 5,
    Private = 6,
    Function = 7,
    Generic = 8,
    PushConstant = 9,
    AtomicCounter = 10,
    Image = 11,
    StorageBuffer = 12,
    PhysicalStorageBuffer = 5349,
};

constexpr bool is_terminator(Op op) {
    switch (op) {
    case Op::Return:
    case Op::ReturnValue:
    case Op::Unreachable:
    case Op::TerminateInvocation:
    case Op::Branch:
    case Op::BranchConditional:
    case Op::Switch:
        return true;
    default:
        return false;
    }
}

} // namespace vull::vk::spv
