#include "Globals.h"

//===========================================================================
// Global Variables (definitions)
//===========================================================================
ServiceType g_serviceType = ServiceKorea;
ServerType  g_serverType  = ServerNormal;
int         g_languageType = 0;
int         g_codePage     = 0;
int         g_version      = 0;

std::string g_accountAddr = "";
std::string g_accountPort = "";
std::string g_regstrationWeb = "";
std::vector<std::string> g_loadingScreenList;

XMLDocument g_xmlDocument;

std::vector<u32> s_dwAdminAID;
std::vector<u32> s_dwYellowAID;
#include "../session/Session.h"

CSession g_session;
