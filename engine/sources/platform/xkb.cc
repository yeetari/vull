#include <vull/platform/xkb.hh>

#include <vull/core/input.hh>

#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

namespace vull::platform {
Key xkb_translate_key(xkb_keysym_t keysym) {
    switch (keysym) {
    case XKB_KEY_F1:
        return Key::F1;
    case XKB_KEY_F2:
        return Key::F2;
    case XKB_KEY_F3:
        return Key::F3;
    case XKB_KEY_F4:
        return Key::F4;
    case XKB_KEY_F5:
        return Key::F5;
    case XKB_KEY_F6:
        return Key::F6;
    case XKB_KEY_F7:
        return Key::F7;
    case XKB_KEY_F8:
        return Key::F8;
    case XKB_KEY_F9:
        return Key::F9;
    case XKB_KEY_F10:
        return Key::F10;
    case XKB_KEY_F11:
        return Key::F11;
    case XKB_KEY_F12:
        return Key::F12;
    case XKB_KEY_a:
        return Key::A;
    case XKB_KEY_b:
        return Key::B;
    case XKB_KEY_c:
        return Key::C;
    case XKB_KEY_d:
        return Key::D;
    case XKB_KEY_e:
        return Key::E;
    case XKB_KEY_f:
        return Key::F;
    case XKB_KEY_g:
        return Key::G;
    case XKB_KEY_h:
        return Key::H;
    case XKB_KEY_i:
        return Key::I;
    case XKB_KEY_j:
        return Key::J;
    case XKB_KEY_k:
        return Key::K;
    case XKB_KEY_l:
        return Key::L;
    case XKB_KEY_m:
        return Key::M;
    case XKB_KEY_n:
        return Key::N;
    case XKB_KEY_o:
        return Key::O;
    case XKB_KEY_p:
        return Key::P;
    case XKB_KEY_q:
        return Key::Q;
    case XKB_KEY_r:
        return Key::R;
    case XKB_KEY_s:
        return Key::S;
    case XKB_KEY_t:
        return Key::T;
    case XKB_KEY_u:
        return Key::U;
    case XKB_KEY_v:
        return Key::V;
    case XKB_KEY_w:
        return Key::W;
    case XKB_KEY_x:
        return Key::X;
    case XKB_KEY_y:
        return Key::Y;
    case XKB_KEY_z:
        return Key::Z;
    case XKB_KEY_space:
        return Key::Space;
    case XKB_KEY_Return:
        return Key::Return;
    case XKB_KEY_Shift_L:
        return Key::Shift;
    default:
        return Key::Unknown;
    }
}
} // namespace vull::platform
