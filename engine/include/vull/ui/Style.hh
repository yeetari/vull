#pragma once

#include <vull/ui/Font.hh>

namespace vull::ui {

class Style {
    Font m_main_font;
    Font m_monospace_font;

public:
    Style(Font &&main_font, Font &&monospace_font)
        : m_main_font(vull::move(main_font)), m_monospace_font(vull::move(monospace_font)) {}

    Font &main_font() { return m_main_font; }
    Font &monospace_font() { return m_monospace_font; }
};

} // namespace vull::ui
