#pragma once
#include "InsultWords.h"
#include <string>
#include <string_view>
#include <vector>

namespace ro {
// Original mode1 tokenizer: CR emits lines; LF only advances the line start;
// '#' emits a token. Only exact leading '//' is a comment. No EOF flush.
std::vector<std::string> TokenizeMannerFile(std::string_view bytes);
std::size_t NextMannerCharacter(std::string_view bytes,std::size_t offset,int codePage);
int SelectedMannerCodePage();
const char* MannerLanguageFolder(unsigned primaryLanguage);

class MannerFilter {
public:
    bool Load(const char* path="manner.txt");
    void AppendEncodedFile(std::string_view bytes,int codePage);
    bool IsBadEncodedSentence(std::string_view bytes,int codePage) const;
    bool IsBadUtf8Sentence(std::string_view text) const;
    bool EnsureLoaded();
    void Clear();
    const InsultWords& EncodedWords() const { return m_encoded; }
    std::size_t UndecodableWords() const { return m_undecodable; }
    bool WasLoadAttempted() const { return m_attempted; }
private:
    InsultWords m_encoded,m_utf8;
    bool m_attempted=false,m_loaded=false;
    std::size_t m_undecodable=0;
};
MannerFilter& GetMannerFilter();
}
