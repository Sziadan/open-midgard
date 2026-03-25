#include "ClientInfoLocale.h"
#include "Globals.h"
#include "File.h"
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

void AddUniqueLoadingScreen(const std::string& path)
{
    if (path.empty()) {
        return;
    }

    const std::string lowered = ToLowerAscii(path);
    for (const std::string& existing : g_loadingScreenList) {
        if (ToLowerAscii(existing) == lowered) {
            return;
        }
    }
    g_loadingScreenList.push_back(path);
}

void AddDefaultLoadingScreensFromDirectory(const char* directory)
{
    if (!directory || !*directory) {
        return;
    }

    char path[260] = {};
    for (int index = 0; index <= 99; ++index) {
        std::snprintf(path, sizeof(path), "%sloading%02d.jpg", directory, index);
        if (g_fileMgr.IsDataExist(path)) {
            AddUniqueLoadingScreen(path);
        }
    }
}

void EnsureDefaultLoadingScreens()
{
    static const char* kUiKor =
        "data\\texture\\"
        "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
        "\\";

    AddDefaultLoadingScreensFromDirectory("data\\texture\\basepic\\");
    AddDefaultLoadingScreensFromDirectory(kUiKor);
}

std::string GetChildContents(XMLElement* element, const char* childName)
{
    if (!element) {
        return {};
    }
    XMLElement* child = element->FindChild(childName);
    if (!child) {
        return {};
    }
    return child->GetContents();
}

bool EqualsIgnoreCase(const std::string& left, const char* right)
{
    if (!right) {
        return false;
    }

    size_t rightLen = std::strlen(right);
    if (left.size() != rightLen) {
        return false;
    }

    for (size_t i = 0; i < rightLen; ++i) {
        if (std::tolower(static_cast<unsigned char>(left[i])) !=
            std::tolower(static_cast<unsigned char>(right[i]))) {
            return false;
        }
    }

    return true;
}

void ParseLoadingImages(XMLElement* element)
{
    if (!element) {
        return;
    }

    XMLElement* loading = element->FindChild("loading");
    if (!loading) {
        return;
    }

    g_loadingScreenList.clear();
    for (XMLElement* image = loading->FindChild("image"); image; image = image->FindNext("image")) {
        const std::string& contents = image->GetContents();
        if (contents.empty()) {
            continue;
        }
        AddUniqueLoadingScreen("texture\\loading\\" + contents);
    }

    EnsureDefaultLoadingScreens();
}

}

//===========================================================================
// Helpers
//===========================================================================
static void SetOption(XMLElement* element) {
    if (!element) {
        return;
    }

    const std::string serviceType = GetChildContents(element, "servicetype");
    if (EqualsIgnoreCase(serviceType, "korea")) g_serviceType = ServiceKorea;
    else if (EqualsIgnoreCase(serviceType, "america")) g_serviceType = ServiceAmerica;
    else if (EqualsIgnoreCase(serviceType, "japan")) g_serviceType = ServiceJapan;
    else if (EqualsIgnoreCase(serviceType, "china")) g_serviceType = ServiceChina;
    else if (EqualsIgnoreCase(serviceType, "taiwan")) g_serviceType = ServiceTaiwan;
    else if (EqualsIgnoreCase(serviceType, "thai")) g_serviceType = ServiceThai;
    else if (EqualsIgnoreCase(serviceType, "indonesia")) g_serviceType = ServiceIndonesia;
    else if (EqualsIgnoreCase(serviceType, "philippine")) g_serviceType = ServicePhilippine;
    else if (EqualsIgnoreCase(serviceType, "malaysia")) g_serviceType = ServiceMalaysia;
    else if (EqualsIgnoreCase(serviceType, "singapore")) g_serviceType = ServiceSingapore;
    else if (EqualsIgnoreCase(serviceType, "germany")) g_serviceType = ServiceGermany;
    else if (EqualsIgnoreCase(serviceType, "india")) g_serviceType = ServiceIndia;
    else if (EqualsIgnoreCase(serviceType, "brazil")) g_serviceType = ServiceBrazil;
    else if (EqualsIgnoreCase(serviceType, "australia")) g_serviceType = ServiceAustralia;
    else if (EqualsIgnoreCase(serviceType, "russia")) g_serviceType = ServiceRussia;
    else if (EqualsIgnoreCase(serviceType, "vietnam")) g_serviceType = ServiceVietnam;
    else if (EqualsIgnoreCase(serviceType, "chile")) g_serviceType = ServiceChile;
    else if (EqualsIgnoreCase(serviceType, "france")) g_serviceType = ServiceFrance;

    const std::string serverType = GetChildContents(element, "servertype");
    if (EqualsIgnoreCase(serverType, "primary")) g_serverType = ServerNormal;
    else if (EqualsIgnoreCase(serverType, "sakray")) g_serverType = ServerSakray;
    else if (EqualsIgnoreCase(serverType, "local")) g_serverType = ServerLocal;
    else if (EqualsIgnoreCase(serverType, "pk")) g_serverType = ServerPK;

    if (element->FindChild("readfolder")) {
        g_readFolderFirst = 1;
    }

    ParseLoadingImages(element);
}

//===========================================================================
// Implementation
//===========================================================================
void InitClientInfo(const char* fileName) {
    std::ifstream file(fileName, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (file.read(buffer.data(), size)) {
        g_xmlDocument.ReadDocument(buffer.data(), buffer.data() + size);
    }

    XMLElement* clientInfo = GetClientInfo();
    SetOption(clientInfo);
    
    // Default selection
    SelectClientInfo(0);
}

XMLElement* GetClientInfo() {
    return g_xmlDocument.m_root.FindChild("clientinfo");
}

void SelectClientInfo(int connectionIndex) {
    XMLElement* clientInfo = GetClientInfo();
    if (!clientInfo) return;

    XMLElement* connection = clientInfo->FindChild("connection");
    int current = 0;
    while (connection && current < connectionIndex) {
        connection = connection->FindNext("connection");
        current++;
    }

    if (!connection) return;

    SetOption(connection);

    XMLElement* addr = connection->FindChild("address");
    if (addr) g_accountAddr = addr->GetContents();

    XMLElement* port = connection->FindChild("port");
    if (port) g_accountPort = port->GetContents();

    XMLElement* version = connection->FindChild("version");
    if (version) g_version = std::atoi(version->GetContents().c_str());

    XMLElement* lang = connection->FindChild("langtype");
    if (lang) g_serviceType = (ServiceType)std::atoi(lang->GetContents().c_str());

    XMLElement* registration = connection->FindChild("registrationweb");
    if (registration) g_regstrationWeb = registration->GetContents();
}

void SelectClientInfo2(int connectionIndex, int subConnectionIndex) {
    // Port of the original SelectClientInfo2 logic if needed for sub-connections
    (void)subConnectionIndex;
    SelectClientInfo(connectionIndex);
}

const std::vector<std::string>& GetLoadingScreenList() {
    EnsureDefaultLoadingScreens();
    return g_loadingScreenList;
}

void RefreshDefaultLoadingScreenList() {
    EnsureDefaultLoadingScreens();
}
