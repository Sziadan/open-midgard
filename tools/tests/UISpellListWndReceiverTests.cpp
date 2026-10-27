#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::filesystem::path FindRepositoryRoot()
{
    std::filesystem::path current = std::filesystem::current_path();
    while (!current.empty()) {
        if (std::filesystem::exists(current / "HighPriest.exe")
            && std::filesystem::exists(current / "Ref" / "UIFrameWnd2.cpp")) {
            return current;
        }
        const std::filesystem::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }

    std::filesystem::path source = std::filesystem::path(__FILE__).parent_path();
    if (source.is_relative()) {
        source = std::filesystem::absolute(source);
    }
    for (int level = 0; level < 4 && !source.empty(); ++level) {
        if (std::filesystem::exists(source / "HighPriest.exe")) {
            return source;
        }
        source = source.parent_path();
    }
    throw std::runtime_error("cannot locate repository root for receiver parity test");
}

const std::filesystem::path& RepositoryRoot()
{
    static const std::filesystem::path root = FindRepositoryRoot();
    return root;
}

void Require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string ReadText(const char* path)
{
    const std::filesystem::path absolutePath = RepositoryRoot() / path;
    std::ifstream file(absolutePath, std::ios::binary);
    Require(static_cast<bool>(file), std::string("cannot read ") + absolutePath.string());
    file.seekg(0, std::ios::end);
    const std::streamoff length = file.tellg();
    Require(length >= 0, std::string("cannot size ") + path);
    std::string text(static_cast<std::size_t>(length), '\0');
    file.seekg(0, std::ios::beg);
    file.read(text.data(), length);
    Require(static_cast<bool>(file), std::string("cannot read contents of ") + absolutePath.string());
    return text;
}

std::vector<std::uint8_t> ReadBytes(const char* path)
{
    const std::filesystem::path absolutePath = RepositoryRoot() / path;
    std::ifstream file(absolutePath, std::ios::binary);
    Require(static_cast<bool>(file), std::string("cannot read ") + absolutePath.string());
    file.seekg(0, std::ios::end);
    const std::streamoff length = file.tellg();
    Require(length >= 0, std::string("cannot size ") + path);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(bytes.data()), length);
    Require(static_cast<bool>(file), std::string("cannot read contents of ") + absolutePath.string());
    return bytes;
}

std::string FunctionBody(const std::string& source, const std::string& start, const std::string& end)
{
    const std::size_t bodyStart = source.find(start);
    Require(bodyStart != std::string::npos, "missing source function marker: " + start);
    const std::size_t bodyEnd = source.find(end, bodyStart + start.size());
    Require(bodyEnd != std::string::npos, "missing source function end marker: " + end);
    return source.substr(bodyStart, bodyEnd - bodyStart);
}

std::size_t FindAfter(const std::string& text, const std::string& needle, std::size_t start, const char* message)
{
    const std::size_t found = text.find(needle, start);
    Require(found != std::string::npos, message);
    return found;
}

void CheckExecutableDispatch()
{
    constexpr std::uint32_t imageBase = 0x400000;
    constexpr std::uint32_t selectorTableVa = 0x48D758;
    constexpr std::uint32_t jumpTableVa = 0x48D744;
    const std::vector<std::uint8_t> exe = ReadBytes("HighPriest.exe");

    struct BranchEvidence {
        int message;
        std::uint8_t selector;
        std::uint32_t target;
        std::vector<std::uint8_t> prefix;
    };
    const BranchEvidence branches[] = {
        {31, 0x02, 0x48D5AD, {0x8B, 0x86, 0xA0, 0x00, 0x00, 0x00, 0x8D, 0x8E,
                              0x98, 0x00, 0x00, 0x00, 0x8D, 0x55, 0x10, 0x53}},
        {60, 0x03, 0x48D67B, {0x8B, 0xCE, 0xE8, 0x4E, 0x1D, 0xFF, 0xFF, 0x8B,
                              0x06, 0x8B, 0xCE, 0xFF, 0x50, 0x04, 0x5F, 0x33}},
    };

    for (const BranchEvidence& branch : branches) {
        const std::size_t selectorOffset = selectorTableVa - imageBase + (branch.message - 6);
        Require(selectorOffset < exe.size(), "message selector lies outside executable");
        const std::uint8_t selector = exe[selectorOffset];
        Require(selector == branch.selector, "message selector differs from audited executable");

        const std::size_t targetOffset = jumpTableVa - imageBase + selector * 4;
        Require(targetOffset + 4 <= exe.size(), "message jump-table entry lies outside executable");
        const std::uint32_t target = static_cast<std::uint32_t>(exe[targetOffset])
            | (static_cast<std::uint32_t>(exe[targetOffset + 1]) << 8)
            | (static_cast<std::uint32_t>(exe[targetOffset + 2]) << 16)
            | (static_cast<std::uint32_t>(exe[targetOffset + 3]) << 24);
        Require(target == branch.target, "message branch target differs from audited executable");

        const std::size_t codeOffset = target - imageBase;
        Require(codeOffset + branch.prefix.size() <= exe.size(), "message branch bytes lie outside executable");
        for (std::size_t i = 0; i < branch.prefix.size(); ++i) {
            Require(exe[codeOffset + i] == branch.prefix[i], "message branch byte prefix differs from executable");
        }
    }
}

void CheckDecompilationAndRef()
{
    const std::string exe = ReadText("HighPriest.exe.c");
    const std::string ref = ReadText("Ref/UIFrameWnd2.cpp");
    const std::string exeBody = FunctionBody(exe, "//----- (0048D570)", "//----- (0048D790)");
    const std::string refBody = FunctionBody(
        ref,
        "int __thiscall UISpellListWnd::SendMsg(",
        "// Original Signature: public: virtual int __thiscall UIStarPlaceAcceptWnd::SendMsg");

    const auto compareBody = [](const std::string& body) {
        const std::size_t case31 = FindAfter(body, "case 31:", 0, "missing message 31 branch");
        const std::size_t case60 = FindAfter(body, "case 60:", case31, "missing message 60 branch");
        const std::size_t insertSpell = FindAfter(body, "std::vector<int>::insert(&this->m_spellList", case31,
                                                  "message 31 does not append spell ID");
        const std::size_t initializeItem = FindAfter(body, "memset(&info, 0, sizeof(info))", case31,
                                                     "message 31 does not default-initialize item record");
        const std::size_t appendItem = FindAfter(body, "std::list<ITEM_INFO>::_Buynode(&this->m_itemInfoList", case31,
                                                 "message 31 does not append item record");
        const std::size_t constructItem = FindAfter(body, "std::_Construct(&v9->_Value, &info)", case31,
                                                    "message 31 does not construct appended record");
        const std::size_t incrementList = FindAfter(body, "++this->m_itemInfoList._Size", case31,
                                                    "message 31 does not update parallel list size");
        const std::size_t sendRefresh = FindAfter(body, "this->UIWindow::SendMsg(this, 0, 60, 0, 0, 0, 0)", case31,
                                                  "message 31 does not dispatch message 60");
        Require(insertSpell < initializeItem && initializeItem < appendItem && appendItem < constructItem
                    && constructItem < incrementList && incrementList < sendRefresh && sendRefresh < case60,
                "EXE/Ref message 31 operation order differs from the audited sequence");

        const std::size_t recalc = FindAfter(body, "UIItemIdentifyWnd::RecalcScrbarPos(this)", case60,
                                             "message 60 does not recalculate scrollbar");
        const std::size_t invalidate = FindAfter(body, "this->UIWindow::Invalidate(this)", case60,
                                                 "message 60 does not invalidate window");
        Require(recalc < invalidate, "message 60 must recalculate scrollbar before invalidation");
    };
    compareBody(exeBody);
    compareBody(refBody);
}

void CheckImplementationOrder()
{
    const std::string implementation = ReadText("src/ui/UISpellListWnd.cpp");
    const std::size_t case31 = FindAfter(implementation, "case 31:", 0, "implementation missing message 31");
    const std::size_t case60 = FindAfter(implementation, "case 60:", case31, "implementation missing message 60");
    const std::size_t appendSpell = FindAfter(implementation, "m_spellList.push_back(static_cast<int>(wparam))", case31,
                                              "implementation does not append spell ID");
    const std::size_t appendItem = FindAfter(implementation, "m_itemInfoList.emplace_back(ITEM_INFO{})", case31,
                                             "implementation does not append default item record");
    const std::size_t sendRefresh = FindAfter(implementation, "SendMsg(this, 60, 0, 0, 0)", case31,
                                              "implementation does not dispatch message 60");
    Require(appendSpell < appendItem && appendItem < sendRefresh && sendRefresh < case60,
            "implementation message 31 operation order differs from audited branch");

    const std::size_t recalc = FindAfter(implementation, "RecalcScrollBarPos();", case60,
                                         "implementation message 60 does not recalculate scrollbar");
    const std::size_t invalidate = FindAfter(implementation, "Invalidate();", case60,
                                             "implementation message 60 does not invalidate window");
    const std::size_t recalcBody = FindAfter(implementation, "void UISpellListWnd::RecalcScrollBarPos()", case60,
                                             "implementation missing scrollbar integration hook");
    const std::size_t callback = FindAfter(implementation, "m_scrollBarRecalcCallback(*this)", recalcBody,
                                           "scrollbar integration hook does not invoke callback");
    Require(recalc < invalidate && invalidate < recalcBody && callback > recalcBody,
            "implementation message 60 refresh order differs from audited branch");
}

} // namespace

int main()
{
    try {
        CheckExecutableDispatch();
        CheckDecompilationAndRef();
        CheckImplementationOrder();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
