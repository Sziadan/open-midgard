// Selected source599630/599940/5999E0. Included inside packet-handler namespace.
void HandleWhisperSettingPc(CGameMode& mode,const PacketView& packet)
{
    if(!packet.data || packet.packetLength!=4)return;
    g_windowMgr.SendMsg(3,0,0,0);
    mode.m_isWaitingWhisperSetting=0;
    const unsigned operation=packet.data[2],result=packet.data[3];
    if(operation>1 || result>2)return;
    static constexpr const char* fallback[2][3]={
        {" is now ignored."," could not be ignored."," could not be ignored."},
        {" is no longer ignored."," could not be allowed."," could not be allowed."}};
    ro::UiMessageStrings table;table.Load();
    const auto text=mode.m_refuseWhisperName+table.Get(193+operation*3+result,fallback[operation][result]);
    g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(text.c_str()),65535);
}

void HandleWhisperSettingState(CGameMode& mode,const PacketView& packet)
{
    if(!packet.data || packet.packetLength!=4)return;
    g_windowMgr.SendMsg(3,0,0,0);
    mode.m_isWaitingWhisperSetting=0;
    const unsigned operation=packet.data[2],result=packet.data[3];
    if(operation>1 || result>1)return;
    static constexpr const char* fallback[2][2]={
        {"All whispers are refused.","Could not refuse all whispers."},
        {"All whispers are accepted.","Could not accept all whispers."}};
    ro::UiMessageStrings table;table.Load();
    const auto text=table.Get(58+operation*2+result,fallback[operation][result]);
    g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(text.c_str()),65535);
}

void HandleWhisperExclusionList(CGameMode&,const PacketView& packet)
{
    if(!packet.data || packet.packetLength<4 || ReadLE16(packet.data+2)!=packet.packetLength)return;
    g_windowMgr.SendMsg(3,0,0,0);
    // The selected code reads the header as signed16 and ignores a partial tail.
    const int count=(static_cast<s16>(ReadLE16(packet.data+2))-4)/24;
    ro::UiMessageStrings table;table.Load();
    const auto header=table.Get(count?63:62,count?"Whisper exclusion list:":"The whisper exclusion list is empty.");
    g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(header.c_str()),16776960);
    for(int i=0;i<count;++i){
        const char* first=reinterpret_cast<const char*>(packet.data+4+i*24);
        const auto end=std::find(first,first+24,'\0');
        const std::string name(first,end);
        g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(name.c_str()),16776960);
    }
}
