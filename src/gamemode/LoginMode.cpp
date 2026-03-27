#include <winsock2.h>
#include "CursorRenderer.h"
#include "LoginMode.h"
#include "ui/UIWindowMgr.h"
#include "ui/UIWaitWnd.h"
#include "render/Renderer.h"
#include "render3d/Device.h"
#include "render3d/RenderDevice.h"
#include "core/ClientInfoLocale.h"
#include "core/File.h"
#include "core/Globals.h"
#include "main/WinMain.h"
#include "network/Connection.h"
#include "network/Packet.h"
#include "audio/Audio.h"
#include "session/Session.h"
#include "res/Sprite.h"
#include "res/ActRes.h"
#include "render/DC.h"
#include "DebugLog.h"
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdio>

namespace {

bool CanReturnToCharacterSelect()
{
    return g_session.m_charServerAddr[0] != '\0' && g_session.m_charServerPort > 0;
}

std::string ChooseLoginWallpaperName()
{
    static const char* kUiKorPrefix =
        "texture\\"
        "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
        "\\";

    const char* baseNames[] = {
        "ad_title.jpg",
        "rag_title.jpg",
        "win_login.bmp",
        "title.bmp",
        "title.jpg",
        "login_background.jpg",
        "login_background.bmp",
        "loginwin.bmp",
        nullptr
    };

    const char* prefixes[] = {
        "",
        "texture\\",
        "texture\\interface\\",
        "texture\\interface\\basic_interface\\",
        "texture\\login_interface\\",
        kUiKorPrefix,
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\login_interface\\",
        "ui\\",
        nullptr
    };

    for (int b = 0; baseNames[b]; ++b) {
        for (int p = 0; prefixes[p]; ++p) {
            std::string candidate = std::string(prefixes[p]) + baseNames[b];
            if (g_fileMgr.IsDataExist(candidate.c_str())) {
                return baseNames[b];
            }
        }
    }

    return "title.bmp";
}

void CopyCString(char* dst, size_t dstSize, const char* src)
{
    if (!dst || dstSize == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    std::strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

} // namespace

CLoginMode::CLoginMode() 
    : m_numServer(0), m_serverSelected(0), m_numChar(0), m_subModeStartTime(0),
      m_syncRequestTime(0), m_wndWait(nullptr), m_multiLang(0), 
      m_nSelectedAccountNo(0), m_nSelectedAccountNo2(0),
    m_zonePort(0)
{
    std::memset(m_charParam, 0, sizeof(m_charParam));
    std::memset(m_makingCharName, 0, sizeof(m_makingCharName));
    std::memset(m_emaiAddress, 0, sizeof(m_emaiAddress));
    std::memset(m_userPassword, 0, sizeof(m_userPassword));
    std::memset(m_userId, 0, sizeof(m_userId));
    std::memset(m_serverInfo, 0, sizeof(m_serverInfo));
    std::memset(m_charInfo, 0, sizeof(m_charInfo));
    std::memset(m_zoneAddr, 0, sizeof(m_zoneAddr));
}

CLoginMode::~CLoginMode() {
}

void CLoginMode::OnInit(const char* worldName) {
    (void)worldName;

    m_loopCond = 1;
    m_isConnected = 0;
    m_nextSubMode = -1;
    m_subModeCnt = 0;

    if (!LoadClientInfoCandidates()) {
        m_strErrorInfo = "Unable to load client info; using fallback account endpoint 127.0.0.1:6900.";
        g_accountAddr = "127.0.0.1";
        g_accountPort = "6900";
    } else if (GetClientInfoConnectionCount() > 1) {
        SetLoginStatus("Login: select a server before connecting.");
    }

    m_wallPaperBmpName = ChooseLoginWallpaperName();
    g_windowMgr.SetLoginWallpaper(m_wallPaperBmpName);
    g_windowMgr.RemoveAllWindows();

    m_subMode = LoginSubMode_Login;
    if (g_session.m_pendingReturnToCharSelect != 0 && CanReturnToCharacterSelect()) {
        m_subMode = LoginSubMode_ConnectChar;
        g_session.m_pendingReturnToCharSelect = 0;
    }
    m_selectedCharIndex = 0;
    m_selectedCharSlot = 0;
    if (GetClientInfoConnectionCount() <= 1) {
        SetLoginStatus("Login: ready.");
    }

    CAudio* audio = CAudio::GetInstance();
    if (audio) {
        audio->PlayBGM("bgm\\01.mp3");
    }

    OnChangeState(m_subMode);
}

void CLoginMode::OnExit() {
    RefreshMainWindowTitle();
}

int CLoginMode::OnRun() {
    if (m_nextSubMode != -1) {
        const int nextSubMode = m_nextSubMode;
        DbgLog("[Login] switching state %d -> %d\n", m_subMode, nextSubMode);
        m_subMode = nextSubMode;
        m_subModeCnt = 0;
        m_nextSubMode = -1;
        OnChangeState(nextSubMode);
    }

    OnUpdate();

    ++m_subModeCnt;
    return 1;
}

void CLoginMode::OnUpdate() {
    if (m_isConnected) {
        PollNetwork();
    }

    g_windowMgr.OnProcess();
    const bool hasLegacyDevice = GetRenderDevice().GetLegacyDevice() != nullptr;
    g_windowMgr.SetComposeCursorState(m_cursorActNum, m_mouseAnimStartTick, !hasLegacyDevice);
    g_windowMgr.OnDraw();
    g_windowMgr.SetComposeCursorState(m_cursorActNum, m_mouseAnimStartTick, false);
    if (hasLegacyDevice) {
        DrawModeCursor(m_cursorActNum, m_mouseAnimStartTick);
    }

    Sleep(16);
}

int CLoginMode::SendMsg(int msg, int wparam, int lparam, int extra) {
    switch (msg) {
    case LoginMsg_RequestConnect:
        m_nextSubMode = LoginSubMode_ConnectAccount;
        SetLoginStatus("Login: connect requested.");
        return 1;

    case LoginMsg_SelectClientInfo:
        if (wparam >= 0 && wparam < GetClientInfoConnectionCount()) {
            SelectClientInfo(wparam);
            const std::vector<ClientInfoConnection>& connections = GetClientInfoConnections();
            const char* display = connections[wparam].display.empty()
                ? connections[wparam].address.c_str()
                : connections[wparam].display.c_str();
            char status[160] = {};
            std::snprintf(status, sizeof(status), "Login: selected server '%s'.", display ? display : "");
            SetLoginStatus(status);
            return 1;
        }
        return 0;

    case LoginMsg_SetPassword:
        CopyCString(m_userPassword, sizeof(m_userPassword), reinterpret_cast<const char*>(wparam));
        return 1;

    case LoginMsg_SetUserId:
        CopyCString(m_userId, sizeof(m_userId), reinterpret_cast<const char*>(wparam));
        return 1;

    case LoginMsg_ReturnToLogin:
    case LoginMsg_Disconnect:
        CRagConnection::instance()->Disconnect();
        m_isConnected = 0;
        m_nextSubMode = LoginSubMode_Login;
        SetLoginStatus("Login: disconnected.");
        return 1;

    case LoginMsg_Quit:
        g_modeMgr.Quit();
        return 1;

    case LoginMsg_RequestAccount:
        SetLoginStatus("Login: account request flow not implemented yet.");
        return 1;

    case LoginMsg_Intro:
        SetLoginStatus("Login: intro action not implemented yet.");
        return 1;

    case LoginMsg_SaveAccount:
        SetLoginStatus(extra != 0 ? "Login: save account enabled." : "Login: save account disabled.");
        return 1;

    case LoginMsg_SetEmail:
        CopyCString(m_emaiAddress, sizeof(m_emaiAddress), reinterpret_cast<const char*>(wparam));
        return 1;

    case LoginMsg_GetEmail:
        return reinterpret_cast<int>(m_emaiAddress);

    case LoginMsg_GetMakingCharName:
        return reinterpret_cast<int>(m_makingCharName);

    case LoginMsg_SetMakingCharName:
        CopyCString(m_makingCharName, sizeof(m_makingCharName), reinterpret_cast<const char*>(wparam));
        return 1;

    case LoginMsg_GetCharParam:
        return reinterpret_cast<int>(m_charParam);

    case LoginMsg_SetCharParam:
        if (wparam != 0) {
            std::memcpy(m_charParam, reinterpret_cast<const void*>(wparam), sizeof(m_charParam));
        }
        return 1;

    case LoginMsg_GetCharInfo:
        return reinterpret_cast<int>(m_charInfo);

    case LoginMsg_GetCharCount:
        return m_numChar;

    case LoginMsg_SelectCharacter:
        if (wparam >= 0 && wparam < m_numChar) {
            m_selectedCharIndex = wparam;
            m_selectedCharSlot = static_cast<int>(m_charInfo[wparam].CharNum);
            g_session.SetSelectedCharacterAppearance(m_charInfo[wparam]);
            m_nextSubMode = LoginSubMode_SelectChar;
            return 1;
        }
        return 0;

    case LoginMsg_RequestMakeCharacter: {
        m_selectedCharSlot = wparam;
        m_nextSubMode = LoginSubMode_MakeChar;
        return 1;
    }

    case LoginMsg_RequestDeleteCharacter: {
        char msg[96];
        std::snprintf(msg, sizeof(msg), "Login: delete-character for slot %d is not implemented yet.", wparam);
        SetLoginStatus(msg);
        return 1;
    }

    case LoginMsg_CreateCharacter: {
        if (!m_isConnected) {
            SetLoginStatus("Login: not connected to the char server.");
            return 0;
        }

        PACKET_CZ_MAKE_CHAR pkt{};
        pkt.PacketType = PACKETID_CH_MAKE_CHAR;
        CopyCString(pkt.name, sizeof(pkt.name), m_makingCharName);
        pkt.Str = static_cast<u8>(m_charParam[0]);
        pkt.Agi = static_cast<u8>(m_charParam[1]);
        pkt.Vit = static_cast<u8>(m_charParam[2]);
        pkt.Int = static_cast<u8>(m_charParam[3]);
        pkt.Dex = static_cast<u8>(m_charParam[4]);
        pkt.Luk = static_cast<u8>(m_charParam[5]);
        pkt.CharNum = static_cast<u8>(m_selectedCharSlot);
        pkt.hairColor = static_cast<u16>(m_charParam[6]);
        pkt.hairStyle = static_cast<u16>(m_charParam[7]);

        CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&pkt), static_cast<int>(sizeof(pkt)));
        DbgLog("[Login] CH_MAKE_CHAR sent: name='%.24s' stats=%d/%d/%d/%d/%d/%d slot=%d hairColor=%d hairStyle=%d\n",
               pkt.name,
               (int)pkt.Str, (int)pkt.Agi, (int)pkt.Vit, (int)pkt.Int, (int)pkt.Dex, (int)pkt.Luk,
               (int)pkt.CharNum, (int)pkt.hairColor, (int)pkt.hairStyle);

        m_wndWait = static_cast<UIWaitWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_WAITWND));
        if (m_wndWait) {
            m_wndWait->SetMsg("Creating character...", 16, 1);
        }

        SetLoginStatus("Login: requesting character creation...");
        return 1;
    }

    case LoginMsg_ReturnToCharSelect:
        m_nextSubMode = LoginSubMode_CharSelect;
        return 1;

    default:
        break;
    }

    return 0;
}

void CLoginMode::OnChangeState(int newState) {
    m_subModeStartTime = GetTickCount();
    g_windowMgr.RemoveAllWindows();
    g_windowMgr.SetLoginWallpaper(m_wallPaperBmpName);
    m_wndWait = nullptr;

    switch (newState) {
    case LoginSubMode_Login: {
        UIWindow* loginWnd = g_windowMgr.MakeWindow(UIWindowMgr::WID_LOGINWND);
        if (loginWnd) {
            loginWnd->SetShow(1);
        }
        if (GetClientInfoConnectionCount() > 1) {
            UIWindow* serverWnd = g_windowMgr.MakeWindow(UIWindowMgr::WID_SELECTSERVERWND);
            if (serverWnd) {
                serverWnd->SetShow(1);
            }
            SetLoginStatus("Login: select a server before connecting.");
        } else {
            SetLoginStatus("Login: ready.");
        }
        break;
    }

    case LoginSubMode_ConnectAccount: {
        const int port = g_accountPort.empty() ? 6900 : std::atoi(g_accountPort.c_str());
        m_wndWait = static_cast<UIWaitWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_WAITWND));
        if (m_wndWait) {
            m_wndWait->SetMsg("Connecting to account server...", 16, 1);
        }

        m_isConnected = CRagConnection::instance()->Connect(
            g_accountAddr.empty() ? "127.0.0.1" : g_accountAddr.c_str(),
            port > 0 ? port : 6900) ? 1 : 0;

        if (!m_isConnected) {
            CRagConnection::instance()->Disconnect();
            SetLoginStatus("Login: account server connection failed.");
            m_nextSubMode = LoginSubMode_Login;
            break;
        }

        // Send CA_LOGIN (0x0064) to account server
        PACKET_CA_LOGIN pkt{};
        pkt.PacketType = 0x0064;
        pkt.Version    = static_cast<u32>(g_version > 0 ? g_version : 6);
        std::strncpy(pkt.ID,     m_userId,       sizeof(pkt.ID)     - 1);
        std::strncpy(pkt.Passwd, m_userPassword, sizeof(pkt.Passwd) - 1);
        pkt.clienttype = 0;
        CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&pkt), static_cast<int>(sizeof(pkt)));
        DbgLog("[Login] CA_LOGIN sent: user='%s' version=%u\n", pkt.ID, pkt.Version);
        SetLoginStatus("Login: waiting for account server response...");
        break;
    }

    case LoginSubMode_ConnectChar: {
        const char* charIp = nullptr;
        int charPort = 0;
        if (m_serverSelected >= 0 && m_serverSelected < m_numServer) {
            const SERVER_ADDR& sv = m_serverInfo[m_serverSelected];
            in_addr addrIn{};
            addrIn.s_addr = sv.ip;
            charIp = inet_ntoa(addrIn);
            charPort = static_cast<int>(static_cast<u16>(sv.port));
        } else if (CanReturnToCharacterSelect()) {
            charIp = g_session.m_charServerAddr;
            charPort = g_session.m_charServerPort;
        }

        if (!charIp || !*charIp || charPort <= 0) {
            SetLoginStatus("Login: no char server to connect to.");
            m_nextSubMode = LoginSubMode_Login;
            break;
        }

        DbgLog("[Login] Connecting to char server: %s:%d (name='%.20s')\n",
               charIp, charPort,
               (m_serverSelected >= 0 && m_serverSelected < m_numServer)
                   ? reinterpret_cast<const char*>(m_serverInfo[m_serverSelected].name)
                   : "return");

        CopyCString(g_session.m_charServerAddr, sizeof(g_session.m_charServerAddr), charIp);
        g_session.m_charServerPort = charPort;

        // Disconnect from account server before connecting to char server.
        CRagConnection::instance()->Disconnect();
        m_isConnected = 0;

        m_wndWait = static_cast<UIWaitWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_WAITWND));
        if (m_wndWait) {
            m_wndWait->SetMsg("Connecting to char server...", 16, 1);
        }

        m_isConnected = CRagConnection::instance()->Connect(charIp, charPort) ? 1 : 0;
        if (!m_isConnected) {
            CRagConnection::instance()->Disconnect();
            SetLoginStatus("Login: char server connection failed.");
            m_nextSubMode = LoginSubMode_Login;
            break;
        }

        // Send CA_ENTER (0x0065) to char server
        PACKET_CA_ENTER ePkt{};
        ePkt.PacketType = 0x0065;
        ePkt.AID        = g_session.m_aid;
        ePkt.AuthCode   = g_session.m_authCode;
        ePkt.UserLevel  = g_session.m_userLevel;
        ePkt.unused     = 0;
        ePkt.Sex        = g_session.m_sex;
        CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&ePkt), static_cast<int>(sizeof(ePkt)));
        DbgLog("[Login] CA_ENTER sent: aid=%u authCode=%u userLevel=%u sex=%d\n",
               ePkt.AID, ePkt.AuthCode, ePkt.UserLevel, (int)ePkt.Sex);
        SetLoginStatus("Login: waiting for character list...");
        break;
    }

    case LoginSubMode_CharSelect: {
        UIWindow* charSelectWnd = g_windowMgr.MakeWindow(UIWindowMgr::WID_SELECTCHARWND);
        if (charSelectWnd) {
            charSelectWnd->SetShow(1);
        }
        g_windowMgr.SetLoginStatus("");
        break;
    }

    case LoginSubMode_MakeChar: {
        UIWindow* makeCharWnd = g_windowMgr.MakeWindow(UIWindowMgr::WID_MAKECHARWND);
        if (makeCharWnd) {
            makeCharWnd->SetShow(1);
        }
        SetLoginStatus("Login: create a new character.");
        break;
    }

    case LoginSubMode_SelectChar: {
        if (m_numChar <= 0 || m_selectedCharIndex < 0 || m_selectedCharIndex >= m_numChar) {
            SetLoginStatus("Login: no characters on this account.");
            m_nextSubMode = LoginSubMode_Login;
            break;
        }

        const u8 charNum = m_charInfo[m_selectedCharIndex].CharNum;
        g_session.SetSelectedCharacterAppearance(m_charInfo[m_selectedCharIndex]);
        DbgLog("[Login] Selecting char slot %d (name='%.24s')\n",
               (int)charNum, reinterpret_cast<const char*>(m_charInfo[m_selectedCharIndex].name));

        PACKET_CZ_SELECT_CHAR selPkt{};
        selPkt.PacketType = 0x0066;
        selPkt.CharNum    = charNum;
        CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&selPkt), static_cast<int>(sizeof(selPkt)));
        SetLoginStatus("Login: selecting character...");
        break;
    }

    case LoginSubMode_ZoneConnect: {
        // Disconnect from char server.
        CRagConnection::instance()->Disconnect();
        m_isConnected = 0;

        DbgLog("[Login] Connecting to zone server: %s:%d map=%s\n",
               m_zoneAddr, m_zonePort, g_session.m_curMap);

        m_wndWait = static_cast<UIWaitWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_WAITWND));
        if (m_wndWait) {
            m_wndWait->SetMsg("Connecting to zone server...", 16, 1);
        }

        m_isConnected = CRagConnection::instance()->Connect(m_zoneAddr, m_zonePort) ? 1 : 0;
        if (!m_isConnected) {
            CRagConnection::instance()->Disconnect();
            SetLoginStatus("Login: zone server connection failed.");
            m_nextSubMode = LoginSubMode_Login;
            break;
        }

        // Send CZ_ENTER2 (0x0436) to zone/map server for packet_ver 23.
        PACKET_CZ_ENTER2 zPkt{};
        zPkt.PacketType  = PacketProfile::ActiveMapServerSend::kWantToConnection;
        zPkt.AID         = g_session.m_aid;
        zPkt.GID         = g_session.m_gid;
        zPkt.AuthCode    = g_session.m_authCode;
        zPkt.ClientTick  = GetTickCount();
        zPkt.Sex         = g_session.m_sex;
        CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&zPkt), static_cast<int>(sizeof(zPkt)));
        DbgLog("[Login] CZ_ENTER2 sent to zone: opcode=0x%04X aid=%u gid=%u\n", zPkt.PacketType, zPkt.AID, zPkt.GID);
        SetLoginStatus("Login: waiting for zone server...");
        break;
    }

    case LoginSubMode_ServerSelect:
    case LoginSubMode_Notice:
    case LoginSubMode_Licence:
    case LoginSubMode_AccountList:
    case LoginSubMode_DeleteChar:
    case LoginSubMode_SubAccount:
    case LoginSubMode_CharSelectReturn:
    default:
        g_windowMgr.MakeWindow(UIWindowMgr::WID_LOGINWND);
        SetLoginStatus("Login: sub-mode not implemented.");
        break;
    }
}

void CLoginMode::SetLoginStatus(const char* status)
{
    if (!status) {
        return;
    }

    if (m_strErrorInfo == status) {
        return;
    }

    m_strErrorInfo = status;
    g_windowMgr.SetLoginStatus(m_strErrorInfo);
    RefreshMainWindowTitle(status);
}

bool CLoginMode::LoadClientInfoCandidates()
{
    const DWORD dataAttrs = GetFileAttributesA("data");
    const bool hasDataFolder = dataAttrs != INVALID_FILE_ATTRIBUTES && (dataAttrs & FILE_ATTRIBUTE_DIRECTORY) != 0;

    std::vector<const char*> candidates;
    if (hasDataFolder) {
        candidates.push_back("data\\clientinfo.xml");
        candidates.push_back("data\\sclientinfo.xml");
    }
    candidates.push_back("data\\clientinfo.xml");
    candidates.push_back("data\\sclientinfo.xml");
    candidates.push_back("clientinfo.xml");
    candidates.push_back("sclientinfo.xml");
    candidates.push_back("System\\clientinfo.xml");
    candidates.push_back("System\\sclientinfo.xml");

    for (const char* path : candidates) {
        if (InitClientInfo(path)) {
            g_session.InitAccountInfo();
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Network polling  – called each frame while the socket is open
// ---------------------------------------------------------------------------
void CLoginMode::PollNetwork()
{
    std::vector<u8> pkt;
    for (int budget = 32; budget > 0; --budget) {
        if (!CRagConnection::instance()->RecvPacket(pkt)) break;
        if (pkt.size() < 2) continue;

        const u16 id = static_cast<u16>(pkt[0]) | (static_cast<u16>(pkt[1]) << 8);
        char status[96];
        std::snprintf(status, sizeof(status), "Login: received packet 0x%04X (%d bytes)", id, static_cast<int>(pkt.size()));
        SetLoginStatus(status);
        switch (id) {
        case 0x0069: OnAcceptLogin(pkt);    break;
        case 0x006A: OnRefuseLogin(pkt);    break;
        case 0x006B: OnAcceptChar(pkt);     break;
        case 0x006C: OnRefuseChar(pkt);     break;
        case 0x006D: OnAcceptMakeChar(pkt); break;
        case 0x006E: OnRefuseMakeChar(pkt); break;
        case 0x0071: OnNotifyZonesvr(pkt);  break;
        case 0x0073:
            OnZcAcceptEnter(pkt);
            return;
        case 0x0081: OnDisconnectMsg(pkt);  break;
        case 0x0283: break;
        case 0x8482: break;
        case 0x8483: break;
        default:
            DbgLog("[Login] Unknown packet 0x%04X (%d bytes)\n", id, (int)pkt.size());
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// 0x0069  AC_ACCEPT_LOGIN  – account server accepted credentials
// Header: type(2)+len(2)+authCode(4)+AID(4)+userLevel(4)+lastIP(4)+lastTime(26)+sex(1) = 47 bytes
// Followed by SERVER_ADDR entries (32 bytes each).
// ---------------------------------------------------------------------------
void CLoginMode::OnAcceptLogin(const std::vector<u8>& raw)
{
    if (raw.size() < 47) {
        SetLoginStatus("Login error: AC_ACCEPT_LOGIN too short.");
        m_nextSubMode = LoginSubMode_Login;
        return;
    }
    const u8* p = raw.data();

    const u16 packetLen = static_cast<u16>(p[2]) | (static_cast<u16>(p[3]) << 8);
    g_session.m_authCode   = u32(p[4])|(u32(p[5])<<8)|(u32(p[6])<<16)|(u32(p[7])<<24);
    g_session.m_aid        = u32(p[8])|(u32(p[9])<<8)|(u32(p[10])<<16)|(u32(p[11])<<24);
    g_session.m_userLevel  = u32(p[12])|(u32(p[13])<<8)|(u32(p[14])<<16)|(u32(p[15])<<24);

    // Sex byte — some regions add 10; normalise to 0/1.
    u8 rawSex = p[46];
    g_session.m_sex = (rawSex >= 10) ? rawSex - 10 : rawSex;

    // Server list: each SERVER_ADDR is 32 bytes, starts at offset 47.
    m_numServer = 0;
    const int serverListBytes = static_cast<int>(packetLen) - 47;
    if (serverListBytes > 0) {
        const int count = serverListBytes / 32;
        const int toRead = (count < 100) ? count : 100;
        // Direct memcpy: SERVER_ADDR fields are in network byte order in the packet,
        // and Types.h/SERVER_ADDR matches the on-wire layout (32 bytes).
        std::memcpy(m_serverInfo, p + 47, static_cast<size_t>(toRead) * 32);
        m_numServer = toRead;
    }

    DbgLog("[Login] AC_ACCEPT_LOGIN: aid=%u authCode=%u userLevel=%u sex=%d servers=%d\n",
           g_session.m_aid, g_session.m_authCode, g_session.m_userLevel,
           (int)g_session.m_sex, m_numServer);
    SetLoginStatus("Login: account accepted by server.");

    if (m_numServer == 0) {
        SetLoginStatus("Login error: no char servers in login response.");
        m_nextSubMode = LoginSubMode_Login;
        return;
    }

    // Auto-select first char server.
    m_serverSelected = 0;
    m_nextSubMode = LoginSubMode_ConnectChar;
}

// ---------------------------------------------------------------------------
// 0x006A  AC_REFUSE_LOGIN  – account server rejected credentials
// ---------------------------------------------------------------------------
void CLoginMode::OnRefuseLogin(const std::vector<u8>& raw)
{
    const u8 code = (raw.size() > 2) ? raw[2] : 0;
    char msg[80];
    switch (code) {
    case 0:  std::snprintf(msg, sizeof(msg), "Login refused: account suspended."); break;
    case 1:  std::snprintf(msg, sizeof(msg), "Login refused: ID not found."); break;
    case 2:  std::snprintf(msg, sizeof(msg), "Login refused: wrong password."); break;
    case 3:  std::snprintf(msg, sizeof(msg), "Login refused: account already logged in."); break;
    case 4:  std::snprintf(msg, sizeof(msg), "Login refused: account not approved."); break;
    default: std::snprintf(msg, sizeof(msg), "Login refused (code %d).", (int)code); break;
    }
    DbgLog("[Login] AC_REFUSE_LOGIN: code=%d\n", (int)code);
    SetLoginStatus(msg);
    CRagConnection::instance()->Disconnect();
    m_isConnected = 0;
    m_nextSubMode = LoginSubMode_Login;
}

// ---------------------------------------------------------------------------
// 0x006B  HC_ACCEPT_ENTER  – char server returned character list
// Header: type(2)+len(2)+billingInfo(12 bytes)+padding = 24 bytes before char data.
// Each CHARACTER_INFO entry is 108 bytes.
// ---------------------------------------------------------------------------
void CLoginMode::OnAcceptChar(const std::vector<u8>& raw)
{
    if (raw.size() < 4) return;
    const u8* p = raw.data();
    const u16 pktLen = static_cast<u16>(p[2]) | (static_cast<u16>(p[3]) << 8);

    // The Ref uses: numChar = (pktLen - 24) / 108 and char data starts at offset 24.
    const int charAreaLen = static_cast<int>(pktLen) - 24;
    m_numChar = 0;
    if (charAreaLen >= 108) {
        const int count = charAreaLen / 108;
        const int toRead = (count < 12) ? count : 12;
        if (static_cast<int>(raw.size()) >= 24 + toRead * 108) {
            std::memcpy(m_charInfo, p + 24, static_cast<size_t>(toRead) * 108);
            m_numChar = toRead;
        }
    }

    DbgLog("[Login] HC_ACCEPT_ENTER: numChar=%d (pktLen=%u)\n", m_numChar, pktLen);
    SetLoginStatus("Login: character list received from char server.");

    if (m_numChar == 0) {
        SetLoginStatus("Login: no characters on account.");
        m_selectedCharIndex = 0;
        m_nextSubMode = LoginSubMode_CharSelect;
        return;
    }

    DbgLog("[Login] First char: name='%.24s' GID=%u slot=%d\n",
           reinterpret_cast<const char*>(m_charInfo[0].name),
           m_charInfo[0].GID, (int)m_charInfo[0].CharNum);

    m_selectedCharIndex = 0;
    m_nextSubMode = LoginSubMode_CharSelect;
}

// ---------------------------------------------------------------------------
// 0x006C  HC_REFUSE_ENTER  – char server refused entry
// ---------------------------------------------------------------------------
void CLoginMode::OnRefuseChar(const std::vector<u8>& raw)
{
    const u8 code = (raw.size() > 2) ? raw[2] : 0;
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Char server refused entry (code %d).", (int)code);
    DbgLog("[Login] HC_REFUSE_ENTER: code=%d\n", (int)code);
    SetLoginStatus(msg);
    CRagConnection::instance()->Disconnect();
    m_isConnected = 0;
    m_nextSubMode = LoginSubMode_Login;
}

// ---------------------------------------------------------------------------
// 0x006D  HC_ACCEPT_MAKECHAR  – char server accepted the new character
// Layout: type(2) + CHARACTER_INFO(108) = 110 bytes for this client family.
// ---------------------------------------------------------------------------
void CLoginMode::OnAcceptMakeChar(const std::vector<u8>& raw)
{
    if (raw.size() < sizeof(PACKET_HC_ACCEPT_MAKECHAR)) {
        SetLoginStatus("Login error: HC_ACCEPT_MAKECHAR too short.");
        m_nextSubMode = LoginSubMode_CharSelect;
        return;
    }

    CHARACTER_INFO created{};
    std::memcpy(&created, raw.data() + sizeof(u16), sizeof(created));

    if (m_numChar >= 0 && m_numChar < static_cast<int>(std::size(m_charInfo))) {
        m_charInfo[m_numChar] = created;
        ++m_numChar;
    }

    DbgLog("[Login] HC_ACCEPT_MAKECHAR: name='%.24s' gid=%u slot=%d totalChars=%d\n",
           reinterpret_cast<const char*>(created.name), created.GID, (int)created.CharNum, m_numChar);
    SetLoginStatus("Login: character created successfully.");
    m_nextSubMode = LoginSubMode_CharSelect;
}

// ---------------------------------------------------------------------------
// 0x006E  HC_REFUSE_MAKECHAR  – char server refused the new character
// ---------------------------------------------------------------------------
void CLoginMode::OnRefuseMakeChar(const std::vector<u8>& raw)
{
    const u8 code = (raw.size() > 2) ? raw[2] : 0;
    const char* reason = nullptr;
    switch (code) {
    case 0:
        reason = "character name already exists";
        break;
    case 1:
        reason = "age restriction";
        break;
    case 2:
        reason = "character deletion restriction";
        break;
    case 3:
        reason = "invalid character slot";
        break;
    case 11:
        reason = "premium service required";
        break;
    default:
        reason = "character creation denied";
        break;
    }

    char msg[96];
    std::snprintf(msg, sizeof(msg), "Login: character creation failed (%s, code %d).", reason, (int)code);
    DbgLog("[Login] HC_REFUSE_MAKECHAR: code=%d (%s)\n", (int)code, reason);
    SetLoginStatus(msg);
    m_nextSubMode = LoginSubMode_CharSelect;
}

// ---------------------------------------------------------------------------
// 0x0071  HC_NOTIFY_ZONESVR  – char server providing zone server address
// Layout: type(2)+GID(4)+mapName(16)+IP(4)+port(2) = 28 bytes
// ---------------------------------------------------------------------------
void CLoginMode::OnNotifyZonesvr(const std::vector<u8>& raw)
{
    if (raw.size() < 28) return;
    const u8* p = raw.data();

    g_session.m_gid = u32(p[2])|(u32(p[3])<<8)|(u32(p[4])<<16)|(u32(p[5])<<24);

    // Map name: 16 bytes starting at offset 6, may include .gat extension.
    char mapName[17] = {};
    std::memcpy(mapName, p + 6, 16);
    mapName[16] = '\0';

    // Strip extension (.gat or .rsw) to get the bare map name.
    char* dot = std::strrchr(mapName, '.');
    if (dot) *dot = '\0';
    std::strncpy(g_session.m_curMap, mapName, sizeof(g_session.m_curMap) - 1);
    g_session.m_curMap[sizeof(g_session.m_curMap) - 1] = '\0';

    // Zone server IP (network byte order in packet; inet_ntoa handles it).
    u32 zoneIpRaw = u32(p[22])|(u32(p[23])<<8)|(u32(p[24])<<16)|(u32(p[25])<<24);
    in_addr za{};
    za.s_addr = zoneIpRaw;
    std::strncpy(m_zoneAddr, inet_ntoa(za), sizeof(m_zoneAddr) - 1);

    // RO packets store this field little-endian on the wire, which lands in host order here.
    const u16 portRaw = static_cast<u16>(p[26]) | (static_cast<u16>(p[27]) << 8);
    m_zonePort = static_cast<int>(portRaw);

    DbgLog("[Login] HC_NOTIFY_ZONESVR: gid=%u map='%s' zone=%s:%d\n",
           g_session.m_gid, g_session.m_curMap, m_zoneAddr, m_zonePort);

    CRagConnection::instance()->Disconnect();
    m_isConnected = 0;
    m_nextSubMode = LoginSubMode_ZoneConnect;
}

// ---------------------------------------------------------------------------
// 0x0073  ZC_ACCEPT_ENTER  – zone/map server accepted, game begins
// Layout: type(2)+serverTick(4)+posX_Y_dir(3 packed bytes)+font(1)+sex(1) = 11 bytes
// ---------------------------------------------------------------------------
void CLoginMode::OnZcAcceptEnter(const std::vector<u8>& raw)
{
    if (raw.size() < 11) return;
    const u8* p = raw.data();

    const u32 serverTick = u32(p[2])|(u32(p[3])<<8)|(u32(p[4])<<16)|(u32(p[5])<<24);
    g_session.SetServerTime(serverTick);

    // Position is packed into 3 bytes (10-bit X, 10-bit Y, 4-bit Dir).
    const int startX   = (int(p[6]) << 2)          | (p[7] >> 6);
    const int startY   = ((p[7] & 0x3F) << 4)      | (p[8] >> 4);
    const int startDir = p[8] & 0x0F;
    g_session.SetPlayerPosDir(startX, startY, startDir);

    char curMap[40];
    std::snprintf(curMap, sizeof(curMap), "%s.rsw", g_session.m_curMap);

    DbgLog("[Login] ZC_ACCEPT_ENTER: map=%s x=%d y=%d dir=%d tick=%u → switching to GameMode\n",
           curMap, startX, startY, startDir, serverTick);

    g_modeMgr.Switch(1, curMap);
}

// ---------------------------------------------------------------------------
// 0x0081  SC_NOTIFY_BAN  – server disconnecting us
// ---------------------------------------------------------------------------
void CLoginMode::OnDisconnectMsg(const std::vector<u8>& raw)
{
    const u8 code = (raw.size() > 2) ? raw[2] : 0;
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Disconnected by server (code %d).", (int)code);
    DbgLog("[Login] SC_NOTIFY_BAN: code=%d\n", (int)code);
    SetLoginStatus(msg);
    CRagConnection::instance()->Disconnect();
    m_isConnected = 0;
    m_nextSubMode = LoginSubMode_Login;
}
