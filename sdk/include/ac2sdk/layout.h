#pragma once
// Helpers for layout-exact mirrors of the game's classes. The game is 32-bit (x86), so the layout checks only
// compile in 32-bit builds (the lab mod); 64-bit builds (the port) skip them and use the same declarations.

#include <cstddef>
#include <cstdint>

#define AC2_CONCAT2(a, b) a##b
#define AC2_CONCAT(a, b) AC2_CONCAT2(a, b)

// Unknown bytes inside a mirrored class: AC2_PAD(0x24);
#define AC2_PAD(n) std::uint8_t AC2_CONCAT(pad_, __LINE__)[n]

#if defined(_M_IX86) || defined(__i386__)
#define AC2_GAME_ABI 1
#define AC2_ASSERT_OFFSET(type, member, off) \
    static_assert(offsetof(type, member) == (off), #type "::" #member " is not at " #off)
#define AC2_ASSERT_SIZE(type, size) static_assert(sizeof(type) == (size), "sizeof(" #type ") != " #size)
#else
#define AC2_GAME_ABI 0
#define AC2_ASSERT_OFFSET(type, member, off) static_assert(true)
#define AC2_ASSERT_SIZE(type, size) static_assert(true)
#endif
