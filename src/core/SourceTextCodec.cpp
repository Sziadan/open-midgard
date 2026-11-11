#include "SourceTextCodec.h"
#include "Globals.h"

#include <limits>
#if !defined(_WIN32)
#include <cerrno>
#include <iconv.h>
#endif

namespace ro {

void DecodeSourceResourceText(char* text)
{
    // Ref/MsgStrings.cpp:101. Korean XML resources use an involutive byte mapping.
    if (g_serviceType != ServiceKorea || !text) return;
    for (; *text; ++text) {
        auto byte = static_cast<unsigned char>(*text);
        if (byte >= 'a' && byte <= 'z') byte = static_cast<unsigned char>(-37 - byte);
        if (byte >= 'A' && byte <= 'Z') byte = static_cast<unsigned char>(-101 - byte);
        else if (byte >= 0xb0 && byte <= 0xbf) byte = static_cast<unsigned char>(111 - byte);
        *text = static_cast<char>(byte);
    }
}

int SelectedSourceTextCodePage()
{
    if (g_codePage) return g_codePage;
    // Ref/Locale.cpp InitLanguage uses the service when selecting CLanguage.
    switch (g_serviceType) {
    case ServiceKorea: return 949;
    case ServiceJapan: return 932;
    case ServiceChina: return 936;
    case ServiceTaiwan: return 950;
    case ServiceThai: return 874;
    case ServiceRussia: return 1251;
    case ServiceVietnam: return 1258;
    case ServiceBrazil:
    case ServiceFrance: return 1252;
    case ServiceChile: return 1145;
    default: return 0;
    }
}

namespace {
#if defined(_WIN32)
std::optional<std::string> Convert(std::string_view input, UINT from, UINT to)
{
    if (input.empty()) return std::string{};
    if (input.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) return std::nullopt;
    const auto length = static_cast<int>(input.size());
    const int count = MultiByteToWideChar(from, MB_ERR_INVALID_CHARS, input.data(), length, nullptr, 0);
    if (count <= 0) return std::nullopt;
    std::wstring wide(static_cast<std::size_t>(count), L'\0');
    if (!MultiByteToWideChar(from, MB_ERR_INVALID_CHARS, input.data(), length, wide.data(), count)) return std::nullopt;
    // The selected client calls WideCharToMultiByte with flags 0 and the
    // operating system's default replacement for unrepresentable characters.
    const int size = WideCharToMultiByte(to, 0, wide.data(), count, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return std::nullopt;
    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (!WideCharToMultiByte(to, 0, wide.data(), count, bytes.data(), size, nullptr, nullptr)) return std::nullopt;
    return bytes;
}
#else
std::string EncodingName(int codePage)
{
    if (codePage == 65001) return "UTF-8";
    if (codePage == 1145) return "IBM1145";
    if (!codePage) return "WINDOWS-1252";
    return "CP" + std::to_string(codePage);
}

std::optional<std::string> Convert(std::string_view input, int from, int to)
{
    if (input.empty()) return std::string{};
    const auto converter = iconv_open(EncodingName(to).c_str(), EncodingName(from).c_str());
    if (converter == reinterpret_cast<iconv_t>(-1)) return std::nullopt;
    struct ConverterGuard {
        iconv_t converter;
        ~ConverterGuard() { iconv_close(converter); }
    } guard{converter};
    std::string source(input), bytes;
    char* cursor = source.data();
    std::size_t remaining = source.size();
    do {
        char buffer[256];
        char* target = buffer;
        std::size_t capacity = sizeof(buffer);
        const auto result = iconv(converter, &cursor, &remaining, &target, &capacity);
        if (result == static_cast<std::size_t>(-1) && errno != E2BIG) return std::nullopt;
        bytes.append(buffer, static_cast<std::size_t>(target - buffer));
    } while (remaining);
    return bytes;
}
#endif
}

std::optional<std::string> EncodeSourceText(std::string_view utf8, int codePage)
{
    return Convert(utf8, 65001, codePage);
}

std::optional<std::string> DecodeSourceText(std::string_view bytes, int codePage)
{
    return Convert(bytes, codePage, 65001);
}

}
