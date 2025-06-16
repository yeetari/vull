#include <vull/support/args_parser.hh>

#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/support/algorithm.hh>
#include <vull/support/function.hh>
#include <vull/support/integral.hh>
#include <vull/support/span.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>

#include <stddef.h>

namespace vull {

ArgsParser::ArgsParser(String name, String description, String version)
    : m_name(vull::move(name)), m_description(vull::move(description)), m_version(vull::move(version)) {
    m_options.push(Option{
        .help_string = "Print this help information",
        .long_name = "help",
        .accept_value =
            [this](StringView program_path, StringView) {
                print_help(program_path);
                return ArgsParseResult::ExitSuccess;
            },
    });
    m_options.push(Option{
        .help_string = "Print version information",
        .long_name = "version",
        .accept_value =
            [this](StringView, StringView) {
                vull::println("{} {}", m_name, m_version);
                return ArgsParseResult::ExitSuccess;
            },
    });
}

template <>
void ArgsParser::add_argument(String &value, String name, bool required) {
    m_arguments.push(Argument{
        .name = vull::move(name),
        .min_values = required ? 1u : 0u,
        .max_values = 1,
        .accept_value =
            [&value](StringView, StringView argument) {
                value = argument;
                return ArgsParseResult::Continue;
            },
    });
}

template <>
void ArgsParser::add_argument(Vector<String> &value, String name, bool required) {
    m_arguments.push(Argument{
        .name = vull::move(name),
        .min_values = required ? 1u : 0u,
        .max_values = UINT32_MAX,
        .accept_value =
            [&value](StringView, StringView argument) {
                value.push(argument);
                return ArgsParseResult::Continue;
            },
    });
}

void ArgsParser::add_flag(bool &present, String help_string, String long_name, char short_name) {
    m_options.push(Option{
        .help_string = vull::move(help_string),
        .long_name = vull::move(long_name),
        .short_name = short_name,
        .accept_value =
            [&present](StringView, StringView) {
                present = true;
                return ArgsParseResult::Continue;
            },
    });
}

template <>
void ArgsParser::add_option(String &value, String help_string, String long_name, char short_name) {
    m_options.push(Option{
        .help_string = vull::move(help_string),
        .long_name = vull::move(long_name),
        .short_name = short_name,
        .has_argument = true,
        .accept_value =
            [&value](StringView, StringView argument) {
                value = argument;
                return ArgsParseResult::Continue;
            },
    });
}

template <Integral T>
void ArgsParser::add_option(T &value, String help_string, String long_name, char short_name) {
    m_options.push(Option{
        .help_string = vull::move(help_string),
        .long_name = long_name,
        .short_name = short_name,
        .has_argument = true,
        .accept_value =
            [&value, long_name = vull::move(long_name)](StringView program_path, StringView argument) {
                auto parsed = argument.to_integral<T>();
                if (!parsed) {
                    vull::println("{}: option '--{}' expects an integer", program_path, long_name);
                    return ArgsParseResult::ExitFailure;
                }
                value = *parsed;
                return ArgsParseResult::Continue;
            },
    });
}

template void ArgsParser::add_option(int8_t &, String, String, char);
template void ArgsParser::add_option(int16_t &, String, String, char);
template void ArgsParser::add_option(int32_t &, String, String, char);
template void ArgsParser::add_option(int64_t &, String, String, char);
template void ArgsParser::add_option(uint8_t &, String, String, char);
template void ArgsParser::add_option(uint16_t &, String, String, char);
template void ArgsParser::add_option(uint32_t &, String, String, char);
template void ArgsParser::add_option(uint64_t &, String, String, char);

String ArgsParser::Argument::to_string() const {
    StringBuilder sb;
    if (min_values > 0) {
        sb.append("<{}>", name);
    } else {
        sb.append("[{}]", name);
    }
    if (max_values > 1) {
        sb.append("...");
    }
    return sb.build();
}

void ArgsParser::print_help(StringView program_path) {
    vull::println("{} - {}\n", m_name, m_description);

    StringBuilder usage_sb;
    usage_sb.append("usage: {} [options] ", program_path);
    for (const auto &argument : m_arguments) {
        usage_sb.append(argument.to_string());
        usage_sb.append(' ');
    }
    vull::println(usage_sb.build());

    vull::println("\noptions:");
    for (const auto &option : m_options) {
        StringBuilder sb;
        sb.append("  ");

        // Print short name or whitespace.
        if (option.short_name != '\0') {
            sb.append('-');
            sb.append(option.short_name);
            sb.append(", ");
        } else {
            sb.append("    ");
        }

        // Print long name.
        sb.append("--");
        sb.append(option.long_name);

        // Print alignment whitespace for help string.
        if (option.long_name.length() >= 20) {
            sb.append('\n');
            sb.append(String::repeated(' ', 28));
        } else {
            sb.append(String::repeated(' ', 20 - option.long_name.length()));
        }

        // Insert alignment on newlines in help string.
        for (char ch : option.help_string) {
            if (ch == '\n') {
                sb.append('\n');
                sb.append(String::repeated(' ', 28));
                continue;
            }
            sb.append(ch);
        }
        vull::println(sb.build());
    }
}

ArgsParseResult ArgsParser::handle_long_option(Option *&option_waiting_for_argument, StringView program_path,
                                               StringView option_name) {
    StringView argument;
    for (size_t i = 0; i < option_name.length(); i++) {
        if (option_name[i] == '=') {
            argument = option_name.substr(i + 1);
            option_name = option_name.substr(0, i);
            break;
        }
    }

    auto *option = vull::find_if(m_options.begin(), m_options.end(), [option_name](const Option &option) {
        return option_name == option.long_name.view();
    });

    if (option == m_options.end()) {
        vull::println("{}: unrecognised option '--{}'", program_path, option_name);
        return ArgsParseResult::ExitFailure;
    }

    if (!option->has_argument && !argument.empty()) {
        vull::println("{}: option '--{}' doesn't allow an argument", program_path, option->long_name);
        return ArgsParseResult::ExitFailure;
    }

    if (option->has_argument && argument.empty()) {
        option_waiting_for_argument = option;
        return ArgsParseResult::Continue;
    }

    return option->accept_value(program_path, argument);
}

ArgsParseResult ArgsParser::handle_short_option(Option *&option_waiting_for_argument, StringView program_path,
                                                char option_name) {
    auto *option = vull::find_if(m_options.begin(), m_options.end(), [option_name](const Option &option) {
        return option_name == option.short_name;
    });

    if (option == m_options.end()) {
        vull::println("{}: unrecognised option '-{c}'", program_path, option_name);
        return ArgsParseResult::ExitFailure;
    }

    if (option->has_argument) {
        option_waiting_for_argument = option;
        return ArgsParseResult::Continue;
    }

    return option->accept_value(program_path, {});
}

ArgsParseResult ArgsParser::parse_args(int argc, const char *const *argv) {
    Vector<StringView> args(argv, argv + argc);
    if (args.empty()) {
        args.push(m_name);
    }

    bool accepting_options = true;
    uint32_t argument_index = 0;
    uint32_t argument_value_count = 0;
    Option *option_waiting_for_argument = nullptr;
    for (const auto &arg : vull::slice(args, 1u)) {
        if (auto *option = vull::exchange(option_waiting_for_argument, nullptr)) {
            if (auto result = option->accept_value(args[0], arg); result != ArgsParseResult::Continue) {
                return result;
            }
            continue;
        }

        if (accepting_options && arg == "--") {
            accepting_options = false;
            continue;
        }

        // Handle long options.
        if (accepting_options && arg.starts_with("--")) {
            if (auto result = handle_long_option(option_waiting_for_argument, args[0], arg.substr(2));
                result != ArgsParseResult::Continue) {
                return result;
            }
            continue;
        }

        // Handle (potentially multiple) short options.
        if (accepting_options && arg.starts_with('-') && arg.length() >= 2) {
            StringView arg_view = arg.substr(1);
            while (!arg_view.empty()) {
                if (auto result = handle_short_option(option_waiting_for_argument, args[0], arg_view[0]);
                    result != ArgsParseResult::Continue) {
                    return result;
                }

                arg_view = arg_view.substr(1);

                // Check if the last option was expecting an argument.
                if (option_waiting_for_argument != nullptr) {
                    // Use remainder of string if there is anything, e.g. -Afoo.
                    if (!arg_view.empty()) {
                        if (auto result = option_waiting_for_argument->accept_value(args[0], arg_view);
                            result != ArgsParseResult::Continue) {
                            return result;
                        }
                        option_waiting_for_argument = nullptr;
                    }
                    break;
                }
            }
            continue;
        }

        // Otherwise we have a positional argument.
        if (argument_index >= m_arguments.size()) {
            vull::println("{}: extraneous argument '{}'", args[0], arg);
            return ArgsParseResult::ExitFailure;
        }

        const auto &argument = m_arguments[argument_index];
        if (auto result = argument.accept_value(args[0], arg); result != ArgsParseResult::Continue) {
            return result;
        }

        // Check if we should increment the argument index.
        if (++argument_value_count >= argument.max_values) {
            argument_index++;
            argument_value_count = 0;
            continue;
        }
    }

    if (option_waiting_for_argument != nullptr) {
        vull::println("{}: option '--{}' requires an argument", args[0], option_waiting_for_argument->long_name);
        return ArgsParseResult::ExitFailure;
    }

    if (argument_index < m_arguments.size() && argument_value_count < m_arguments[argument_index].min_values) {
        vull::println("{}: missing value for argument {}", args[0], m_arguments[argument_index].to_string());
        return ArgsParseResult::ExitFailure;
    }

    return ArgsParseResult::Continue;
}

} // namespace vull
