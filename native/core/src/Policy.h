#pragma once
#include <cstdint>
#include <string_view>
namespace food {
enum class Phase { cancelled=0, open=1, selected=2, committed=3, committing=4 };
inline std::uint64_t Hash(std::string_view text,std::uint64_t hash=14695981039346656037ull) {
    for (unsigned char ch:text) { hash^=ch; hash*=1099511628211ull; } return hash;
}
}
