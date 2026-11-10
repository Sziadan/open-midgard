#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ro {
// Positional message table: known empty strings differ from missing entries.
class UiMessageStrings {
public:
    void Parse(std::string_view text) {
        m_rows.clear();
        std::size_t begin=0;
        while (begin<text.size()) {
            const auto end=text.find('#',begin);
            if (end==std::string_view::npos) break;
            while (begin<end && (text[begin]=='\r' || text[begin]=='\n')) ++begin;
            m_rows.emplace_back(text.substr(begin,end-begin));
            begin=end+1;
        }
    }
    bool Load();
    std::optional<std::string_view> Find(unsigned index) const {
        if(index>=m_rows.size())return std::nullopt;
        return std::string_view(m_rows[index]);
    }
    std::string Get(unsigned index,const char* fallback) const {
        return index<m_rows.size() ? m_rows[index] : std::string(fallback);
    }
private:
    std::vector<std::string> m_rows;
};
} // namespace ro
