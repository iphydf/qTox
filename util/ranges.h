/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2024-2025 The TokTok team.
 */

#pragma once

#include <iterator>

// std::views is not available in Android NDK r21d.
namespace qtox::views {
template <typename T>
struct reverse_view
{
    const T& iterable;

    auto begin()
    {
        return std::rbegin(iterable);
    }
    auto end()
    {
        return std::rend(iterable);
    }
};

template <typename T>
reverse_view<T> reverse(const T& iterable)
{
    return {iterable};
}
} // namespace qtox::views
