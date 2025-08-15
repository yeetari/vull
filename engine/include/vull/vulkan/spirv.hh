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
    ViewportIndex = 10,
    FragCoord = 15,
    PointCoord = 16,
    FrontFacing = 17,
    SampleMask = 20,
    FragDepth = 22,
    HelperInvocation = 23,
    NumWorkgroups = 24,
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
    Float16 = 9,
    Float64 = 10,
    Int64 = 11,
    Int64Atomics = 12,
    AtomicStorage = 21,
    Int16 = 22,
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
    InputAttachmentArrayDynamicIndexing = 5303,
    UniformTexelBufferArrayDynamicIndexing = 5304,
    StorageTexelBufferArrayDynamicIndexing = 5305,
    UniformBufferArrayNonUniformIndexing = 5306,
    SampledImageArrayNonUniformIndexing = 5307,
    StorageBufferArrayNonUniformIndexing = 5308,
    StorageImageArrayNonUniformIndexing = 5309,
    InputAttachmentArrayNonUniformIndexing = 5310,
    UniformTexelBufferArrayNonUniformIndexing = 5311,
    StorageTexelBufferArrayNonUniformIndexing = 5312,
    VulkanMemoryModel = 5345,
    PhysicalStorageBufferAddresses = 5347,
    DemoteToHelperInvocation = 5379,
};

enum class Decoration : uint32_t {
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
    Centroid = 16,
    Invariant = 18,
    Restrict = 19,
    Aliased = 20,
    NonWritable = 24,
    NonReadable = 25,
    Location = 30,
    Component = 31,
    Index = 32,
    Binding = 33,
    DescriptorSet = 34,
    Offset = 35,
    NoContraction = 42,
    InputAttachmentIndex = 43,
    NoSignedWrap = 4469,
    NoUnsignedWrap = 4470,
    NonUniform = 5300,
    RestrictPointer = 5355,
    AliasedPointer = 5356,
};

enum class Dim : uint32_t {
    _1D = 0,
    _2D = 1,
    _3D = 2,
    Cube = 3,
    Rect = 4,
    Buffer = 5,
    SubpassData = 6,
};

enum class ExecutionMode : uint32_t {
    PixelCenterInteger = 6,
    OriginUpperLeft = 7,
    OriginLowerLeft = 8,
    EarlyFragmentTests = 9,
    DepthReplacing = 12,
    DepthGreater = 14,
    DepthLess = 15,
    DepthUnchanged = 16,
    LocalSize = 17,
    LocalSizeId = 38,
};

enum class ExecutionModel : uint32_t {
    Vertex = 0,
    Fragment = 4,
    GLCompute = 5,
};

enum class FunctionControlMask : uint32_t {
    None = 0x0,
    Inline = 0x1,
    DontInline = 0x2,
    Pure = 0x4,
    Const = 0x8,
};

enum class GroupOperation : uint32_t {
    Reduce = 0,
    InclusiveScan = 1,
    ExclusiveScan = 2,
    ClusteredReduce = 3,
};

enum class ImageFormat : uint32_t {
    Unknown = 0,
    Rgba32f = 1,
    Rgba16f = 2,
    R32f = 3,
    Rgba8 = 4,
    Rgba8Snorm = 5,
    Rg32f = 6,
    Rg16f = 7,
    R11fG11fB10f = 8,
    R16f = 9,
    Rgba16 = 10,
    Rgb10A2 = 11,
    Rg16 = 12,
    Rg8 = 13,
    R16 = 14,
    R8 = 15,
    Rgba16Snorm = 16,
    Rg16Snorm = 17,
    Rg8Snorm = 18,
    R16Snorm = 19,
    R8Snorm = 20,
    Rgba32i = 21,
    Rgba16i = 22,
    Rgba8i = 23,
    R32i = 24,
    Rg32i = 25,
    Rg16i = 26,
    Rg8i = 27,
    R16i = 28,
    R8i = 29,
    Rgba32ui = 30,
    Rgba16ui = 31,
    Rgba8ui = 32,
    R32ui = 33,
    Rgb10a2ui = 34,
    Rg32ui = 35,
    Rg16ui = 36,
    Rg8ui = 37,
    R16ui = 38,
    R8ui = 39,
};

enum class ImageOperandMask : uint32_t {
    None = 0x0,
    Bias = 0x1,
    Lod = 0x2,
    Grad = 0x4,
    ConstOffset = 0x8,
    Offset = 0x10,
    ConstOffsets = 0x20,
    Sample = 0x40,
    MinLod = 0x80,
    MakeTexelAvailable = 0x100,
    MakeTexelVisible = 0x200,
    NonPrivateTexel = 0x400,
    VolatileTexel = 0x800,
    SignExtend = 0x1000,
    ZeroExtend = 0x2000,
    Nontemporal = 0x4000,
};

enum class LoopControlMask : uint32_t {
    None = 0x0,
    Unroll = 0x1,
    DontUnroll = 0x2,
    DependencyInfinite = 0x4,
    DependencyLength = 0x8,
    MinIterations = 0x10,
    MaxIterations = 0x20,
    IterationMultiple = 0x40,
    PeelCount = 0x80,
    PartialCount = 0x100,
};

enum class MemoryAccessMask : uint32_t {
    None = 0x0,
    Volatile = 0x1,
    Aligned = 0x2,
    Nontemporal = 0x4,
    MakePointerAvailable = 0x8,
    MakePointerVisible = 0x10,
    NonPrivatePointer = 0x20,
};

enum class MemoryModel : uint32_t {
    Glsl450 = 1,
    Vulkan = 3,
};

enum class MemorySemanticsMask : uint32_t {
    Relaxed = 0x0,
    Acquire = 0x2,
    Release = 0x4,
    AcquireRelease = 0x8,
    SequentiallyConsistent = 0x10,
    UniformMemory = 0x40,
    SubgroupMemory = 0x80,
    WorkgroupMemory = 0x100,
    AtomicCounterMemory = 0x400,
    ImageMemory = 0x800,
    OutputMemory = 0x1000,
    MakeAvailable = 0x2000,
    MakeVisible = 0x4000,
    Volatile = 0x8000,
};

enum class Op : uint32_t {
    // Miscellaneous Instructions.
    Nop = 0,
    Undef = 1,

    // Debug Instructions.
    Name = 5,

    // Annotation Instructions.
    Decorate = 71,
    MemberDecorate = 72,

    // Extension Instructions.
    Extension = 10,
    ExtInstImport = 11,
    ExtInst = 12,

    // Mode-Setting Instructions.
    MemoryModel = 14,
    EntryPoint = 15,
    ExecutionMode = 16,
    Capability = 17,
    ExecutionModeId = 331,

    // Type-Declaration Instructions.
    TypeVoid = 19,
    TypeBool = 20,
    TypeInt = 21,
    TypeFloat = 22,
    TypeVector = 23,
    TypeMatrix = 24,
    TypeImage = 25,
    TypeSampler = 26,
    TypeSampledImage = 27,
    TypeArray = 28,
    TypeRuntimeArray = 29,
    TypeStruct = 30,
    TypePointer = 32,
    TypeFunction = 33,
    TypeForwardPointer = 39,

    // Constant-Creation Instructions.
    ConstantTrue = 41,
    ConstantFalse = 42,
    Constant = 43,
    ConstantComposite = 44,
    ConstantNull = 46,
    SpecConstantTrue = 48,
    SpecConstantFalse = 49,
    SpecConstant = 50,
    SpecConstantComposite = 51,
    SpecConstantOp = 52,

    // Memory Instructions.
    Variable = 59,
    ImageTexelPointer = 60,
    Load = 61,
    Store = 62,
    CopyMemory = 63,
    AccessChain = 65,
    PtrAccessChain = 67,
    ArrayLength = 68,
    PtrEqual = 401,
    PtrNotEqual = 402,
    PtrDiff = 403,

    // Function Instructions.
    Function = 54,
    FunctionParameter = 55,
    FunctionEnd = 56,
    FunctionCall = 57,

    // Image Instructions.
    SampledImage = 86,
    ImageSampleImplicitLod = 87,
    ImageSampleExplicitLod = 88,
    ImageSampleDrefImplicitLod = 89,
    ImageSampleDrefExplicitLod = 90,
    ImageSampleProjImplicitLod = 91,
    ImageSampleProjExplicitLod = 92,
    ImageSampleProjDrefImplicitLod = 93,
    ImageSampleProjDrefExplicitLod = 94,
    ImageFetch = 95,
    ImageGather = 96,
    ImageDrefGather = 97,
    ImageRead = 98,
    ImageWrite = 99,
    Image = 100,
    ImageQuerySizeLod = 103,
    ImageQuerySize = 104,
    ImageQueryLod = 105,
    ImageQueryLevels = 106,
    ImageQuerySamples = 107,

    // Conversion Instructions.
    ConvertFToU = 109,
    ConvertFToS = 110,
    ConvertSToF = 111,
    ConvertUToF = 112,
    UConvert = 113,
    SConvert = 114,
    FConvert = 115,
    QuantizeToF16 = 116,
    ConvertPtrToU = 117,
    ConvertUToPtr = 120,
    Bitcast = 124,

    // Composite Instructions.
    VectorExtractDynamic = 77,
    VectorInsertDynamic = 78,
    VectorShuffle = 79,
    CompositeConstruct = 80,
    CompositeExtract = 81,
    CompositeInsert = 82,
    CopyObject = 83,
    Transpose = 84,
    CopyLogical = 400,

    // Arithmetic Instructions.
    SNegate = 126,
    FNegate = 127,
    IAdd = 128,
    FAdd = 129,
    ISub = 130,
    FSub = 131,
    IMul = 132,
    FMul = 133,
    UDiv = 134,
    SDiv = 135,
    FDiv = 136,
    UMod = 137,
    SRem = 138,
    SMod = 139,
    FRem = 140,
    FMod = 141,
    VectorTimesScalar = 142,
    MatrixTimesScalar = 143,
    VectorTimesMatrix = 144,
    MatrixTimesVector = 145,
    MatrixTimesMatrix = 146,
    OuterProduct = 147,
    Dot = 148,

    // Bit Instructions.
    ShiftRightLogical = 194,
    ShiftRightArithmetic = 195,
    ShiftLeftLogical = 196,
    BitwiseOr = 197,
    BitwiseXor = 198,
    BitwiseAnd = 199,
    Not = 200,
    BitFieldInsert = 201,
    BitFieldSExtract = 202,
    BitFieldUExtract = 203,
    BitReverse = 204,
    BitCount = 205,

    // Relational and Logical Instructions.
    Any = 154,
    All = 155,
    IsNan = 156,
    IsInf = 157,
    LogicalEqual = 164,
    LogicalNotEqual = 165,
    LogicalOr = 166,
    LogicalAnd = 167,
    LogicalNot = 168,
    Select = 169,
    IEqual = 170,
    INotEqual = 171,
    UGreaterThan = 172,
    SGreaterThan = 173,
    UGreaterThanEqual = 174,
    SGreaterThanEqual = 175,
    ULessThan = 176,
    SLessThan = 177,
    ULessThanEqual = 178,
    SLessThanEqual = 179,
    FOrdEqual = 180,
    FUnordEqual = 181,
    FOrdNotEqual = 182,
    FUnordNotEqual = 183,
    FOrdLessThan = 184,
    FUnordLessThan = 185,
    FOrdGreaterThan = 186,
    FUnordGreaterThan = 187,
    FOrdLessThanEqual = 188,
    FUnordLessThanEqual = 189,
    FOrdGreaterThanEqual = 190,
    FUnordGreaterThanEqual = 191,

    // Derivative Instructions.
    DPdx = 207,
    DPdy = 208,
    Fwidth = 209,

    // Control-Flow Instructions.
    Phi = 245,
    LoopMerge = 246,
    SelectionMerge = 247,
    Label = 248,
    Branch = 249,
    BranchConditional = 250,
    Switch = 251,
    Return = 253,
    ReturnValue = 254,
    Unreachable = 255,
    TerminateInvocation = 4416,
    DemoteToHelperInvocation = 5380,

    // Atomic Instructions.
    AtomicLoad = 227,
    AtomicStore = 228,
    AtomicExchange = 229,
    AtomicCompareExchange = 230,
    AtomicIIncrement = 232,
    AtomicIDecement = 233,
    AtomicIAdd = 234,
    AtomicISub = 235,
    AtomicSMin = 236,
    AtomicUMin = 237,
    AtomicSMax = 238,
    AtomicUMax = 239,
    AtomicAnd = 240,
    AtomicOr = 241,
    AtomicXor = 242,

    // Barrier Instructions.
    ControlBarrier = 224,
    MemoryBarrier = 225,

    // Non-Uniform Instructions.
    GroupNonUniformElect = 333,
    GroupNonUniformAll = 334,
    GroupNonUniformAny = 335,
    GroupNonUniformAllEqual = 336,
    GroupNonUniformBroadcast = 337,
    GroupNonUniformBroadcastFirst = 338,
    GroupNonUniformBallot = 339,
    GroupNonUniformInverseBallot = 340,
    GroupNonUniformBallotBitExtract = 341,
    GroupNonUniformBallotBitCount = 342,
    GroupNonUniformBallotFindLSB = 343,
    GroupNonUniformBallotFindMSB = 344,
    GroupNonUniformShuffle = 345,
    GroupNonUniformShuffleXor = 346,
    GroupNonUniformShuffleUp = 347,
    GroupNonUniformShuffleDown = 348,
    GroupNonUniformIAdd = 349,
    GroupNonUniformFAdd = 350,
    GroupNonUniformIMul = 351,
    GroupNonUniformFMul = 352,
    GroupNonUniformSMin = 353,
    GroupNonUniformUMin = 354,
    GroupNonUniformFMin = 355,
    GroupNonUniformSMax = 356,
    GroupNonUniformUMax = 357,
    GroupNonUniformFMax = 358,
    GroupNonUniformBitwiseAnd = 359,
    GroupNonUniformBitwiseOr = 360,
    GroupNonUniformBitwiseXor = 361,
    GroupNonUniformLogicalAnd = 362,
    GroupNonUniformLogicalOr = 363,
    GroupNonUniformLogicalXor = 364,
    GroupNonUniformQuadBroadcast = 365,
    GroupNonUniformQuadSwap = 366,
};

enum class Scope : uint32_t {
    Workgroup = 2,
    Subgroup = 3,
    Invocation = 4,
    QueueFamily = 5,
};

enum class SelectionControlMask : uint32_t {
    None = 0x0,
    Flatten = 0x1,
    DontFlatten = 0x2,
};

enum class StorageClass : uint32_t {
    UniformConstant = 0,
    Input = 1,
    Uniform = 2,
    Output = 3,
    Workgroup = 4,
    Private = 6,
    Function = 7,
    PushConstant = 9,
    AtomicCounter = 10,
    Image = 11,
    StorageBuffer = 12,
    PhysicalStorageBuffer = 5349,
};

constexpr FunctionControlMask operator&(FunctionControlMask lhs, FunctionControlMask rhs) {
    return static_cast<FunctionControlMask>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

constexpr FunctionControlMask operator|(FunctionControlMask lhs, FunctionControlMask rhs) {
    return static_cast<FunctionControlMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr ImageOperandMask operator&(ImageOperandMask lhs, ImageOperandMask rhs) {
    return static_cast<ImageOperandMask>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

constexpr ImageOperandMask operator|(ImageOperandMask lhs, ImageOperandMask rhs) {
    return static_cast<ImageOperandMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr LoopControlMask operator&(LoopControlMask lhs, LoopControlMask rhs) {
    return static_cast<LoopControlMask>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

constexpr LoopControlMask operator|(LoopControlMask lhs, LoopControlMask rhs) {
    return static_cast<LoopControlMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr MemoryAccessMask operator&(MemoryAccessMask lhs, MemoryAccessMask rhs) {
    return static_cast<MemoryAccessMask>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

constexpr MemoryAccessMask operator|(MemoryAccessMask lhs, MemoryAccessMask rhs) {
    return static_cast<MemoryAccessMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr MemorySemanticsMask operator&(MemorySemanticsMask lhs, MemorySemanticsMask rhs) {
    return static_cast<MemorySemanticsMask>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

constexpr MemorySemanticsMask operator|(MemorySemanticsMask lhs, MemorySemanticsMask rhs) {
    return static_cast<MemorySemanticsMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr SelectionControlMask operator&(SelectionControlMask lhs, SelectionControlMask rhs) {
    return static_cast<SelectionControlMask>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

constexpr SelectionControlMask operator|(SelectionControlMask lhs, SelectionControlMask rhs) {
    return static_cast<SelectionControlMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

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
