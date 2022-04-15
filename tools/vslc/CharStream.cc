#include "CharStream.hh"

#include <vull/support/Assert.hh>

#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>

CharStream::CharStream(const vull::String &path) : m_file(fopen(path.data(), "rb")) {
    VULL_ENSURE(m_file != nullptr);

    struct stat stat {};
    VULL_ENSURE(fstat(fileno(m_file), &stat) >= 0);
    const auto file_size = static_cast<size_t>(stat.st_size);
    m_data = {static_cast<const char *>(mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fileno(m_file), 0)),
              file_size};
    VULL_ENSURE(m_data.data() != MAP_FAILED); // NOLINT
}

CharStream::CharStream(CharStream &&other) {
    m_file = vull::exchange(other.m_file, nullptr);
    m_data = vull::exchange(other.m_data, {});
    m_position = vull::exchange(other.m_position, 0u);
    m_line = vull::exchange(other.m_line, 1u);
    m_column = vull::exchange(other.m_column, 1u);
}

CharStream::~CharStream() {
    if (m_file != nullptr) {
        // NOLINTNEXTLINE
        munmap(const_cast<char *>(m_data.data()), m_data.size());
        fclose(m_file);
    }
}

bool CharStream::has_next() const {
    return m_position < m_data.size();
}

char CharStream::peek() const {
    return m_data[m_position];
}

char CharStream::next() {
    VULL_ASSERT(has_next());
    if (peek() == '\n') {
        m_line++;
        m_column = 1;
    } else {
        m_column++;
    }
    return m_data[m_position++];
}

const char *CharStream::pointer() const {
    return &m_data[m_position];
}
