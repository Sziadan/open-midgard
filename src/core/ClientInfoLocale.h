#pragma once
#include "Xml.h"

//===========================================================================
// Client info locale functions
//===========================================================================
void InitClientInfo(const char* fileName);
XMLElement* GetClientInfo();

void SelectClientInfo(int connectionIndex);
void SelectClientInfo2(int connectionIndex, int subConnectionIndex);
const std::vector<std::string>& GetLoadingScreenList();
void RefreshDefaultLoadingScreenList();
