#include "Globals.h"

//===========================================================================
// Global Variables (definitions)
//===========================================================================
ServiceType g_serviceType = ServiceKorea;
bool g_multiLang=false;
s8 g_MessageHi=0;
u8 NameBalloonShorten=0,NameBalloonfontBold=0;
bool g_isGoodSkillHanging=false;
ServerType  g_serverType  = ServerNormal;
int         g_languageType = 0;
int         g_codePage     = 0;
int         g_version      = 0;
int         g_passwordEncrypt = 0;
int         g_passwordEncrypt2 = 0;

std::string g_accountAddr = "";
std::string g_accountPort = "";
std::string g_regstrationWeb = "";
std::vector<std::string> g_loadingScreenList;

XMLDocument g_xmlDocument;

std::vector<u32> s_dwAdminAID;
std::vector<u32> s_dwYellowAID;
#include "../session/Session.h"

CSession g_session;
