#include <vull/script/Vm.hh>

#include <vull/core/Log.hh>
#include <vull/script/Bytecode.hh>
#include <vull/script/ConstantPool.hh>
#include <vull/script/Value.hh>
#include <vull/support/Enum.hh>
#include <vull/support/Span.hh>
#include <vull/support/StringView.hh>

#include <stdint.h>

namespace vull::script {

void Vm::dump_frame(Frame &frame) {
    for (const auto inst : frame.m_insts) {
        switch (inst.opcode()) {
            using enum Opcode;
        case OP_loadk: {
            const auto index = static_cast<uint16_t>((inst.b() << 8u) | inst.c());
            vull::println("loadk {} {} ; {}", inst.a(), index, m_constant_pool[index].number);
            break;
        }
        case OP_return0:
            vull::println("return");
            break;
        case OP_return1:
            vull::println("return {}", inst.a());
            break;
        default:
            vull::println("{} {} {} {}", vull::enum_name<1>(inst.opcode()).substr(3), inst.a(), inst.b(), inst.c());
            break;
        }
    }
}

Value Vm::exec_frame(Frame &frame) {
    while (true) {
        const auto inst = *frame.m_ip++;
        switch (inst.opcode()) {
            using enum Opcode;
#define VM_CASE(opcode) case opcode:
#include <script/Opcodes.in>
        default:
            // TODO: This generates better code on GCC and clang < 15, but enables some very aggressive optimisation on
            //       clang 15.
            __builtin_unreachable();
        }
    }
}

} // namespace vull::script
