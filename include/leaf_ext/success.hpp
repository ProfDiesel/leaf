#pragma once

#include <boost/leaf/result.hpp>

#include <type_traits>

namespace leaf_ext {

template<typename T>
inline boost::leaf::result<std::decay_t<T>> success(T &&value) noexcept
{
    return {std::forward<T>(value)};
}

inline boost::leaf::result<void> success() noexcept
{
    return {};
}

}
