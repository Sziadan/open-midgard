#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace ro {

int SelectedSourceTextCodePage();
std::optional<std::string> EncodeSourceText(std::string_view utf8, int codePage);
std::optional<std::string> DecodeSourceText(std::string_view bytes, int codePage);

}
