#pragma once

#include <vull/container/array.hh>
#include <vull/support/assert.hh>
#include <vull/support/utility.hh>

// NOLINTBEGIN
namespace vull {

template <typename>
struct union_tag_t {};

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
    template <typename T, typename... Args>
    Union(union_tag_t<T>, Args &&...args) {
        set<T>(forward<Args>(args)...);
    }
    ~Union() = default;

    Union &operator=(const Union &) = delete;
    Union &operator=(Union &&) = delete;

    template <typename T, typename... Args>
    void set(Args &&...args) {
        new (data) T(forward<Args>(args)...);
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
