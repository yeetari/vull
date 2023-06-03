#pragma once

#include <vull/support/Span.hh>
#include <vull/support/String.hh>

#include <stdio.h>

class CharStream {
    FILE *m_file;
    vull::Span<const char> m_data;
    size_t m_position{0};
    size_t m_line{1};
    size_t m_column{1};

public:
    explicit CharStream(const vull::String &path);
    CharStream(const CharStream &) = delete;
    CharStream(CharStream &&);
    ~CharStream();

    CharStream &operator=(const CharStream &) = delete;
    CharStream &operator=(CharStream &&) = delete;

    bool has_next() const;
    char peek() const;
    char next();
    const char *pointer() const;
};
