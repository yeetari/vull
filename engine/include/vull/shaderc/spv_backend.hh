#pragma once

namespace vull::shaderc::hir {

class Root;

} // namespace vull::shaderc::hir

namespace vull::shaderc::spv {

class Builder;

void build_spv(Builder &builder, const hir::Root &root);

} // namespace vull::shaderc::spv
