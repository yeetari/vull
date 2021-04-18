#pragma once

#include <fstream>
#include <string>
#include <unordered_map>

class Config {
    std::fstream m_file;
    std::unordered_map<std::string, std::string> m_options;

    void write_default_config();

public:
    explicit Config(const char *path);

    void parse();

    template <typename T>
    T get(const char *option) const;
};
