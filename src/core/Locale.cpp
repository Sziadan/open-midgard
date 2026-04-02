#include "ClientInfoLocale.h"
#include "../DebugLog.h"
#include "Globals.h"
#include "File.h"
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace {

std::vector<ClientInfoConnection> g_clientInfoConnections;
int g_selectedClientInfoIndex = 0;

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

#if !RO_PLATFORM_WINDOWS
std::string NormalizePortablePath(const char* path)
{
    if (!path) {
        return {};
    }

    std::string normalized(path);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}
#endif

bool LoadDiskFile(const char* fileName, std::vector<char>* outBuffer)
{
    if (!fileName || !*fileName || !outBuffer) {
        return false;
    }

#if RO_PLATFORM_WINDOWS
    std::ifstream file(fileName, std::ios::binary | std::ios::ate);
#else
    const std::string normalizedPath = NormalizePortablePath(fileName);
    std::ifstream file(normalizedPath, std::ios::binary | std::ios::ate);
#endif
    if (!file.is_open()) {
        return false;
    }

    const std::streamsize size = file.tellg();
    if (size <= 0) {
        return false;
    }

    outBuffer->resize(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    return file.read(outBuffer->data(), size).good();
}

bool BuildExecutableRelativePath(const char* relativePath, char* outPath, size_t outPathSize)
{
    if (!relativePath || !*relativePath || !outPath || outPathSize == 0) {
        return false;
    }

    char modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return false;
    }

    char* lastBackslash = std::strrchr(modulePath, '\\');
    char* lastForwardSlash = std::strrchr(modulePath, '/');
    char* lastSlash = lastBackslash;
    if (!lastSlash || (lastForwardSlash && lastForwardSlash > lastSlash)) {
        lastSlash = lastForwardSlash;
    }
    if (!lastSlash) {
        return false;
    }
    lastSlash[1] = '\0';

#if RO_PLATFORM_WINDOWS
    const int written = std::snprintf(outPath, outPathSize, "%s%s", modulePath, relativePath);
#else
    std::string normalizedRelative(relativePath);
    std::replace(normalizedRelative.begin(), normalizedRelative.end(), '\\', '/');
    const int written = std::snprintf(outPath, outPathSize, "%s%s", modulePath, normalizedRelative.c_str());
#endif
    return written > 0 && static_cast<size_t>(written) < outPathSize;
}

bool TryLoadExecutableRelativeFile(const char* relativePath, std::vector<char>* outBuffer)
{
#if !RO_PLATFORM_WINDOWS
    if (LoadDiskFile(relativePath, outBuffer)) {
        return true;
    }
#endif

    char absolutePath[MAX_PATH] = {};
    if (!BuildExecutableRelativePath(relativePath, absolutePath, sizeof(absolutePath))) {
        return false;
    }
    return LoadDiskFile(absolutePath, outBuffer);
}

bool HasLocalDataDirectory()
{
#if !RO_PLATFORM_WINDOWS
    const DWORD runtimeAttrs = GetFileAttributesA("data");
    if (runtimeAttrs != INVALID_FILE_ATTRIBUTES && (runtimeAttrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return true;
    }
#endif

    char dataPath[MAX_PATH] = {};
    if (!BuildExecutableRelativePath("data", dataPath, sizeof(dataPath))) {
        return false;
    }

    const DWORD attrs = GetFileAttributesA(dataPath);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool ShouldPreferLocalDataFile(const char* fileName)
{
    if (!fileName || !*fileName || !HasLocalDataDirectory()) {
        return false;
    }

    return _strnicmp(fileName, "data\\", 5) == 0 || _strnicmp(fileName, "data/", 5) == 0;
}

bool LoadClientInfoBytes(const char* fileName, std::vector<char>* outBuffer)
{
    if (!fileName || !*fileName || !outBuffer) {
        return false;
    }

    outBuffer->clear();

    if (ShouldPreferLocalDataFile(fileName)) {
        if (TryLoadExecutableRelativeFile(fileName, outBuffer) || LoadDiskFile(fileName, outBuffer)) {
            DbgLog("[ClientInfo] Loaded local file first: %s\n", fileName);
            return true;
        }
    }

    int size = 0;
    unsigned char* bytes = g_fileMgr.GetData(fileName, &size);
    if (bytes && size > 0) {
        outBuffer->assign(reinterpret_cast<const char*>(bytes), reinterpret_cast<const char*>(bytes) + size);
        delete[] bytes;
        DbgLog("[ClientInfo] Loaded from GRF/fallback: %s size=%d\n", fileName, size);
        return true;
    }
    delete[] bytes;

    if (LoadDiskFile(fileName, outBuffer)) {
        DbgLog("[ClientInfo] Loaded fallback disk path: %s\n", fileName);
        return true;
    }

    return false;
}

void ParseAidList(XMLElement* parent, std::vector<u32>* outList)
{
    if (!outList) {
        return;
    }

    outList->clear();
    if (!parent) {
        return;
    }

    for (XMLElement* entry = parent->FindChild("admin"); entry; entry = entry->FindNext("admin")) {
        const std::string& contents = entry->GetContents();
        if (contents.empty()) {
            continue;
        }
        outList->push_back(static_cast<u32>(std::strtoul(contents.c_str(), nullptr, 10)));
    }

    std::sort(outList->begin(), outList->end());
    outList->erase(std::unique(outList->begin(), outList->end()), outList->end());
}

void ParseClientInfoConnections(XMLElement* clientInfo)
{
    g_clientInfoConnections.clear();
    if (!clientInfo) {
        return;
    }

    for (XMLElement* connection = clientInfo->FindChild("connection"); connection; connection = connection->FindNext("connection")) {
        ClientInfoConnection info;
        info.display = GetChildContents(connection, "display");
        info.desc = GetChildContents(connection, "desc");
        info.address = GetChildContents(connection, "address");
        info.port = GetChildContents(connection, "port");
        info.registrationWeb = GetChildContents(connection, "registrationweb");

        const std::string version = GetChildContents(connection, "version");
        if (!version.empty()) {
            info.version = std::atoi(version.c_str());
        }

        const std::string langType = GetChildContents(connection, "langtype");
        if (!langType.empty()) {
            info.langType = std::atoi(langType.c_str());
        }

        g_clientInfoConnections.push_back(info);
    }
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
bool InitClientInfo(const char* fileName) {
    std::vector<char> buffer;
    if (!LoadClientInfoBytes(fileName, &buffer) || buffer.empty()) {
        DbgLog("[ClientInfo] Failed to load candidate: %s\n", fileName ? fileName : "(null)");
        return false;
    }

    if (!g_xmlDocument.ReadDocument(buffer.data(), buffer.data() + buffer.size())) {
        return false;
    }

    XMLElement* clientInfo = GetClientInfo();
    if (!clientInfo) {
        return false;
    }

    ParseClientInfoConnections(clientInfo);
    DbgLog("[ClientInfo] Parsed %d connection entries from %s\n", static_cast<int>(g_clientInfoConnections.size()), fileName ? fileName : "(null)");
    SetOption(clientInfo);
    g_selectedClientInfoIndex = 0;
    SelectClientInfo(0);
    return true;
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

    g_selectedClientInfoIndex = current;

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

    ParseAidList(connection->FindChild("aid"), &s_dwAdminAID);
    XMLElement* yellow = connection->FindChild("yellow");
    if (yellow) {
        ParseAidList(yellow, &s_dwYellowAID);
    } else {
        s_dwYellowAID = s_dwAdminAID;
    }
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

const std::vector<ClientInfoConnection>& GetClientInfoConnections()
{
    return g_clientInfoConnections;
}

int GetClientInfoConnectionCount()
{
    return static_cast<int>(g_clientInfoConnections.size());
}

int GetSelectedClientInfoIndex()
{
    return g_selectedClientInfoIndex;
}

bool IsGravityAid(unsigned int aid)
{
    return std::binary_search(s_dwAdminAID.begin(), s_dwAdminAID.end(), static_cast<u32>(aid));
}

bool IsNameYellow(unsigned int aid)
{
    return std::binary_search(s_dwYellowAID.begin(), s_dwYellowAID.end(), static_cast<u32>(aid));
}
