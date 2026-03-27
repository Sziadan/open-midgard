#pragma once
#include "Xml.h"
#include <string>
#include <vector>

struct ClientInfoConnection {
	std::string display;
	std::string desc;
	std::string address;
	std::string port;
	std::string registrationWeb;
	int version = 0;
	int langType = -1;
};

//===========================================================================
// Client info locale functions
//===========================================================================
bool InitClientInfo(const char* fileName);
XMLElement* GetClientInfo();

void SelectClientInfo(int connectionIndex);
void SelectClientInfo2(int connectionIndex, int subConnectionIndex);
const std::vector<std::string>& GetLoadingScreenList();
void RefreshDefaultLoadingScreenList();
const std::vector<ClientInfoConnection>& GetClientInfoConnections();
int GetClientInfoConnectionCount();
int GetSelectedClientInfoIndex();
bool IsGravityAid(unsigned int aid);
bool IsNameYellow(unsigned int aid);
