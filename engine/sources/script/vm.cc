#include <vull/script/vm.hh>

#include <vull/core/log.hh>
#include <vull/script/bytecode.hh>
#include <vull/script/constant_pool.hh>
#include <vull/script/value.hh>
#include <vull/support/enum.hh>
#include <vull/support/span.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>

#include <stdint.h>

namespace vull::script {

void Vm::dump_frame(Frame &frame) {
    for (uint32_t pc = 0; pc < frame.m_insts.size(); pc++) {
        const auto inst = frame.m_insts[pc];
        vull::print("{d4 }: ", pc);
        switch (inst.opcode()) {
            using enum Opcode;
        case OP_neg:
            vull::println("r{} = neg r{}", inst.a(), inst.b());
            break;
        case OP_jmp:
            vull::println("jmp {} ; to pc {}", inst.sj(), static_cast<int32_t>(pc) + inst.sj());
            break;
        case OP_iseq:
            vull::println("iseq r{} r{}", inst.a(), inst.b());
            break;
        case OP_islt:
            vull::println("islt r{} r{}", inst.a(), inst.b());
            break;
        case OP_isle:
            vull::println("isle r{} r{}", inst.a(), inst.b());
            break;
        case OP_loadk: {
            const auto index = static_cast<uint16_t>((inst.b() << 8u) | inst.c());
            vull::println("r{} = loadk #{} ; {}", inst.a(), index, m_constant_pool[index].number);
            break;
        }
        case OP_return0:
            vull::println("return");
            break;
        case OP_return1:
            vull::println("return r{}", inst.a());
            break;
        default:
            vull::println("r{} = {} r{} r{}", inst.a(), vull::enum_name<1>(inst.opcode()).substr(3), inst.b(),
                          inst.c());
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
#include "opcodes.in"
        default:
            // TODO: This generates better code on GCC and clang < 15, but enables some very aggressive optimisation on
            //       clang 15.
            vull::unreachable();
        }
    }
}

} // namespace vull::script
