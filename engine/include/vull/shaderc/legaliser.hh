#pragma once

#include <vull/shaderc/error.hh>
#include <vull/shaderc/hir.hh>
#include <vull/support/result.hh>

namespace vull::shaderc::ast {

class Root;

} // namespace vull::shaderc::ast

namespace vull::shaderc {

Result<hir::Root, Error> legalise(const ast::Root &ast_root);

} // namespace vull::shaderc
