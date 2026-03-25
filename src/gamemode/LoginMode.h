#pragma once
#include "Mode.h"
#include <vector>
#include <string>
#include <cstdint>

// Forward declarations
class UIWaitWnd;

// Internal account-list entry used by the login UI (distinct from session::accountInfo).
struct LoginAccountInfo {
    int  accountId;
    char accountName[24];
};

struct BILLING_INFO {
    u32 code;
    u32 time1;
    u32 time2;
};

//===========================================================================
// CLoginMode  –  Handles account login and character selection
//===========================================================================
class CLoginMode : public CMode {
public:
    enum LoginMessage {
        LoginMsg_Quit = 2,
        LoginMsg_ReturnToLogin = 30,
        LoginMsg_RequestConnect = 10000,
        LoginMsg_SetPassword = 10007,
        LoginMsg_SetUserId = 10008,
        LoginMsg_Disconnect = 10011,
        LoginMsg_SetEmail = 10012,
        LoginMsg_GetEmail = 10013,
        LoginMsg_GetMakingCharName = 10014,
        LoginMsg_SetMakingCharName = 10015,
        LoginMsg_GetCharParam = 10016,
        LoginMsg_SetCharParam = 10017,
        LoginMsg_RequestAccount = 10018,
        LoginMsg_SaveAccount = 10030,
        LoginMsg_Intro = 10031,
        LoginMsg_GetCharInfo = 10032,
        LoginMsg_GetCharCount = 10033,
        LoginMsg_SelectCharacter = 10034,
        LoginMsg_RequestMakeCharacter = 10035,
        LoginMsg_RequestDeleteCharacter = 10036,
        LoginMsg_CreateCharacter = 10037,
        LoginMsg_ReturnToCharSelect = 10038
    };

    enum LoginSubMode {
        LoginSubMode_Notice = 0,
        LoginSubMode_Licence = 1,
        LoginSubMode_AccountList = 2,
        LoginSubMode_Login = 3,
        LoginSubMode_ConnectAccount = 4,
        LoginSubMode_ConnectChar = 5,
        LoginSubMode_ServerSelect = 6,
        LoginSubMode_CharSelect = 7,
        LoginSubMode_MakeChar = 8,
        LoginSubMode_SelectChar = 9,
        LoginSubMode_DeleteChar = 10,
        LoginSubMode_ZoneConnect = 12,
        LoginSubMode_SubAccount = 13,
        LoginSubMode_CharSelectReturn = 19
    };

    CLoginMode();
    virtual ~CLoginMode();

    virtual void OnInit(const char* worldName) override;
    virtual void OnExit() override;
    virtual int  OnRun() override;
    virtual void OnUpdate() override;
    virtual int  SendMsg(int msg, int wparam, int lparam, int extra) override;
    virtual void OnChangeState(int newState) override;

    // Memory layout from HighPriest.exe.h:31161
    int m_charParam[8];
    char m_makingCharName[64];
    char m_emaiAddress[128];
    char m_userPassword[64];
    char m_userId[64];
    int m_numServer;
    int m_serverSelected;
    int m_numChar;
    int m_selectedCharIndex;
    int m_selectedCharSlot;
    u32 m_subModeStartTime;
    SERVER_ADDR m_serverInfo[100];
    CHARACTER_INFO m_charInfo[12];
    std::string m_wallPaperBmpName;
    BILLING_INFO m_billingInfo;
    u32 m_syncRequestTime;
    UIWaitWnd* m_wndWait;
    std::vector<LoginAccountInfo> m_accountInfo;
    std::vector<LoginAccountInfo> m_accountInfo2;
    u8 m_multiLang;
    int m_nSelectedAccountNo;
    int m_nSelectedAccountNo2;
    std::string m_strErrorInfo;

private:
    void SetLoginStatus(const char* status);
    bool LoadClientInfoCandidates();

    // Network polling — called each frame while connected
    void PollNetwork();

    // Packet handlers (called from PollNetwork)
    void OnAcceptLogin(const std::vector<u8>& pkt);   // 0x0069
    void OnRefuseLogin(const std::vector<u8>& pkt);   // 0x006A
    void OnAcceptChar(const std::vector<u8>& pkt);    // 0x006B
    void OnRefuseChar(const std::vector<u8>& pkt);    // 0x006C
    void OnAcceptMakeChar(const std::vector<u8>& pkt); // 0x006D
    void OnRefuseMakeChar(const std::vector<u8>& pkt); // 0x006E
    void OnNotifyZonesvr(const std::vector<u8>& pkt); // 0x0071
    void OnZcAcceptEnter(const std::vector<u8>& pkt); // 0x0073
    void OnDisconnectMsg(const std::vector<u8>& pkt); // 0x0081

    // Zone server endpoint (populated from HC_NOTIFY_ZONESVR)
    char m_zoneAddr[64];
    int  m_zonePort;
};
