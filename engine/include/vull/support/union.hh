#pragma once

#include <vull/container/array.hh>
#include <vull/support/assert.hh>
#include <vull/support/utility.hh>

// NOLINTBEGIN
namespace vull {

template <typename... Ts>
struct Union {
    static consteval size_t largest_type_size() {
        size_t largest = 0;
        for (size_t size : Array{sizeof(Ts)...}) {
            if (size > largest) {
                largest = size;
            }
        }
        return largest;
    }

    alignas(Ts...) unsigned char data[largest_type_size()];

    Union() = default;
    template <ContainsType<Ts...> T>
    Union(const T &value) {
        set(value);
    }
    template <ContainsType<Ts...> T>
    Union(T &&value) {
        set(move(value));
    }
    ~Union() = default;

    Union &operator=(const Union &) = delete;
    Union &operator=(Union &&) = delete;

    template <ContainsType<Ts...> T>
    void set(const T &value) {
        new (data) T(value);
    }
    template <ContainsType<Ts...> T>
    void set(T &&value) {
        new (data) T(move(value));
    }
    template <ContainsType<Ts...> T>
    void release() {
        get<T>().~T();
    }

    template <ContainsType<Ts...> T>
    T &get() {
        return *__builtin_launder(reinterpret_cast<T *>(data));
    }
    template <ContainsType<Ts...> T>
    const T &get() const {
        return const_cast<Union<Ts...> *>(this)->get<T>();
    }
};

} // namespace vull
// NOLINTEND
