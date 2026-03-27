#pragma once
#include <string>
#include <vector>

//===========================================================================
// XMLElement  –  Represents a single XML node
//===========================================================================
class XMLElement {
public:
    XMLElement(const char* name = nullptr);
    ~XMLElement();

    XMLElement* FindChild(const char* name);
    XMLElement* FindNext(const char* name);
    const std::string& GetContents();
    
    void AddChild(XMLElement* element);
    void AddNext(XMLElement* element);
    void Clear();

public:
    std::string m_name;
    const char* m_start;   // Start of element data (after tag)
    const char* m_end;     // End of element data (before closing tag)
    std::string m_contents; // Cached/parsed contents
    XMLElement* m_next;
    XMLElement* m_child;
};

//===========================================================================
// XMLDocument  –  Handles parsing of a full XML buffer
//===========================================================================
class XMLDocument {
public:
    XMLDocument();
    ~XMLDocument();

    bool ReadDocument(const char* document, const char* end);
    void Clear();

protected:
    const char* ReadElement(XMLElement* parent, const char* document, const char* end);
    const char* ReadContents(XMLElement* element, const char* document, const char* end);

public:
    XMLElement m_root;
    char* m_buf; // Local copy of the document buffer
};
