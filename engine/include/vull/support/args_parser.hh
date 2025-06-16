#pragma once

#include <vull/container/vector.hh>
#include <vull/support/function.hh>
#include <vull/support/integral.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>

#include <stdint.h>

namespace vull {

enum class ArgsParseResult {
    Continue,
    ExitFailure,
    ExitSuccess,
};

class ArgsParser {
    struct Argument {
        String name;
        uint32_t min_values;
        uint32_t max_values;
        Function<ArgsParseResult(StringView, StringView)> accept_value;

        String to_string() const;
    };

    struct Option {
        String help_string;
        String long_name;
        char short_name;
        bool has_argument;
        Function<ArgsParseResult(StringView, StringView)> accept_value;
    };

private:
    String m_name;
    String m_description;
    String m_version;
    Vector<Argument> m_arguments;
    Vector<Option> m_options;

    void print_help(StringView program_path);
    ArgsParseResult handle_long_option(Option *&option_waiting_for_argument, StringView program_path,
                                       StringView option_name);
    ArgsParseResult handle_short_option(Option *&option_waiting_for_argument, StringView program_path,
                                        char option_name);

public:
    ArgsParser(String name, String description, String version);

    template <typename T>
    void add_argument(T &value, String name, bool required);
    void add_flag(bool &present, String help_string, String long_name, char short_name = '\0');
    template <typename T>
    void add_option(T &value, String help_string, String long_name, char short_name = '\0');
    template <Integral T>
    void add_option(T &value, String help_string, String long_name, char short_name = '\0');
    ArgsParseResult parse_args(int argc, const char *const *argv);
};

} // namespace vull
