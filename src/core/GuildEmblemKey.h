#pragma once
#include <cstdint>
namespace ro::guild {
constexpr bool IsEmblemKey(std::uint32_t color) {
    return ((color>>16)&255)>=240 && ((color>>8)&255)<16 && (color&255)>=240;
}
}
