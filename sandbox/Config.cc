#include "Config.hh"

#include <vull/support/Assert.hh>
#include <vull/support/Log.hh>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <utility>

Config::Config(const char *path) : m_file(path, std::ios::in) {
    if (!m_file) {
        Log::info("sandbox", "Config file not found, creating default config");
        m_file.open(path, std::ios::out);
        write_default_config();
        m_file.flush();
        m_file.close();
        m_file.open(path, std::ios::in);
    }
}

void Config::write_default_config() {
    m_file << "window_width: 800\n";
    m_file << "window_height: 600\n";
    m_file << "window_fullscreen: false\n";
    m_file << "# Choose between low_latency, low_power, normal and no_vsync.\n";
    m_file << "swapchain_mode: normal\n";
}

void Config::parse() {
    ENSURE(m_file);
    std::string line;
    while (std::getline(m_file, line)) {
        // Ignore comments.
        if (line.starts_with('#')) {
            continue;
        }
        const auto colon_position = line.find_first_of(':');
        auto key = line.substr(0, colon_position);
        auto val = line.substr(colon_position + 1);
        val.erase(std::remove_if(val.begin(), val.end(), isspace), val.end());
        m_options.emplace(std::move(key), std::move(val));
    }
}

template <>
const std::string &Config::get(const char *option) const {
    return m_options.at(option);
}

template <>
bool Config::get(const char *option) const {
    return m_options.at(option) == "true";
}

template <>
std::uint32_t Config::get(const char *option) const {
    return std::stoi(m_options.at(option));
}
