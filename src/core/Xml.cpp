#include "Xml.h"
#include <cstring>
#include <iostream>

//===========================================================================
// XMLElement
//===========================================================================
XMLElement::XMLElement(const char* name)
    : m_name(name), m_start(nullptr), m_end(nullptr), m_next(nullptr), m_child(nullptr)
{}

XMLElement::~XMLElement() {
    Clear();
}

void XMLElement::Clear() {
    if (m_child) {
        delete m_child;
        m_child = nullptr;
    }
    if (m_next) {
        delete m_next;
        m_next = nullptr;
    }
}

XMLElement* XMLElement::FindChild(const char* name) {
    for (XMLElement* c = m_child; c; c = c->m_next) {
        if (c->m_name && std::strcmp(c->m_name, name) == 0)
            return c;
    }
    return nullptr;
}

XMLElement* XMLElement::FindNext(const char* name) {
    for (XMLElement* n = m_next; n; n = n->m_next) {
        if (n->m_name && std::strcmp(n->m_name, name) == 0)
            return n;
    }
    return nullptr;
}

const std::string& XMLElement::GetContents() {
    if (m_contents.empty() && m_start && m_end) {
        // Parse contents from memory range [m_start, m_end)
        // This is a simplified version of the original's GetContents
        const char* p = m_start;
        while (p < m_end) {
            // Skip inner tags for contents
            if (*p == '<') {
                while (p < m_end && *p != '>') p++;
                if (p < m_end) p++;
                continue;
            }
            if (*p != '\r' && *p != '\n') {
                m_contents += *p;
            }
            p++;
        }
    }
    return m_contents;
}

void XMLElement::AddChild(XMLElement* element) {
    if (!m_child) {
        m_child = element;
    } else {
        m_child->AddNext(element);
    }
}

void XMLElement::AddNext(XMLElement* element) {
    XMLElement* last = this;
    while (last->m_next) last = last->m_next;
    last->m_next = element;
}

//===========================================================================
// XMLDocument
//===========================================================================
XMLDocument::XMLDocument() : m_buf(nullptr) {}

XMLDocument::~XMLDocument() {
    Clear();
}

void XMLDocument::Clear() {
    m_root.Clear();
    if (m_buf) {
        delete[] m_buf;
        m_buf = nullptr;
    }
}

bool XMLDocument::ReadDocument(const char* document, const char* end) {
    Clear();
    size_t size = end - document;
    m_buf = new char[size + 1];
    std::memcpy(m_buf, document, size);
    m_buf[size] = 0;

    m_root.m_start = m_buf;
    m_root.m_end = m_buf + size;

    ReadContents(&m_root, m_buf, m_buf + size);
    return m_root.m_child != nullptr;
}

const char* XMLDocument::ReadContents(XMLElement* element, const char* document, const char* end) {
    const char* p = document;
    while (p < end) {
        if (*p == '<') {
            if (p + 1 < end && p[1] == '/') {
                // Closing tag: check if it matches current element
                const char* tagStart = p + 2;
                const char* tagEnd = tagStart;
                while (tagEnd < end && *tagEnd != '>' && *tagEnd != ' ' && *tagEnd != '\t') tagEnd++;
                
                // Temporarily null-terminate for strcmp (original did this by writing 0 to the buffer)
                // Since this is our local copy m_buf, we can modify it.
                char* modifiableTagEnd = const_cast<char*>(tagEnd);
                char oldChar = *modifiableTagEnd;
                *modifiableTagEnd = 0;
                
                if (element->m_name && std::strcmp(tagStart, element->m_name) == 0) {
                    *modifiableTagEnd = oldChar;
                    element->m_end = p;
                    return tagEnd + 1;
                }
                *modifiableTagEnd = oldChar;
                p = tagEnd + 1;
            } else {
                // Opening tag or comment
                p = ReadElement(element, p, end);
            }
        } else {
            p++;
        }
    }
    return p;
}

const char* XMLDocument::ReadElement(XMLElement* parent, const char* document, const char* end) {
    if (document[1] == '?' || document[1] == '!') {
        // Skip metadata/comments
        const char* p = document;
        while (p < end && *p != '>') p++;
        return (p < end) ? p + 1 : end;
    }

    // Parse tag name
    const char* tagStart = document + 1;
    const char* tagNameEnd = tagStart;
    while (tagNameEnd < end && *tagNameEnd != '>' && *tagNameEnd != '/' && *tagNameEnd != ' ' && *tagNameEnd != '\t') tagNameEnd++;

    char* modifiableTagNameEnd = const_cast<char*>(tagNameEnd);
    char oldChar = *modifiableTagNameEnd;
    *modifiableTagNameEnd = 0;
    
    XMLElement* element = new XMLElement(tagStart);
    *modifiableTagNameEnd = oldChar;
    
    parent->AddChild(element);

    const char* tagClosing = tagNameEnd;
    while (tagClosing < end && *tagClosing != '>') tagClosing++;
    if (tagClosing >= end) return end;

    const char* nextPos = tagClosing + 1;

    if (*(tagClosing - 1) == '/') {
        // Self-closing <tag />
        element->m_start = nullptr;
        element->m_end = nullptr;
        return nextPos;
    } else {
        // Normal <tag>contents</tag>
        element->m_start = nextPos;
        return ReadContents(element, nextPos, end);
    }
}
