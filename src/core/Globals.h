#pragma once
#include <string>
#include <vector>
#include "Types.h"
#include "Xml.h"

//===========================================================================
// Service & Server Types
//===========================================================================
enum ServiceType {
    ServiceKorea = 0,
    ServiceAmerica = 1,
    ServiceJapan = 3,
    ServiceChina = 4,
    ServiceTaiwan = 5,
    ServiceThai = 7,
    ServiceIndonesia = 12,
    ServicePhilippine = 15,
    ServiceMalaysia = 16,
    ServiceSingapore = 17,
    ServiceGermany = 20,
    ServiceIndia = 21,
    ServiceBrazil = 22,
    ServiceAustralia = 23,
    ServiceRussia = 25,
    ServiceVietnam = 26,
    ServiceChile = 30,
    ServiceFrance = 31,
};

enum ServerType {
    ServerNormal = 0,
    ServerSakray = 1,
    ServerLocal  = 2,
    ServerInstantEvent = 3,
    ServerPK = 4,
};

//===========================================================================
// Global Variables (extern declarations)
//===========================================================================
extern ServiceType g_serviceType;
extern ServerType  g_serverType;
extern int         g_languageType;
extern int         g_codePage;
extern int         g_version;

extern std::string g_accountAddr;
extern std::string g_accountPort;
extern std::string g_regstrationWeb;
extern std::vector<std::string> g_loadingScreenList;

extern XMLDocument g_xmlDocument;

// Admin/Yellow AIDs
extern std::vector<u32> s_dwAdminAID;
extern std::vector<u32> s_dwYellowAID;
// Session
class CSession;
extern CSession g_session;
