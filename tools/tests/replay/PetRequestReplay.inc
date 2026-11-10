    {
        // Real source mode recipients and socket peer. These are prerequisites
        // for the pet window/menu, not certification of those absent callers.
        const auto priorPet=g_session.m_petStatus;
        for(const std::string name:std::array<std::string,6>{{"Poring","",std::string(23,'A'),std::string(30,'B'),
                std::string(22,'C')+std::string("\x81\x40",2),std::string("Pet\0ignored",11)}}){
            Require(mode.SendMsg(158,reinterpret_cast<msgparam_t>(name.c_str()),0,0)==1,"pet rename sender failed");
            const auto actual=peer.ReceiveExact(26);
            Require(actual[0]==0xa5 && actual[1]==1,"pet rename used wrong opcode");
            const auto count=(std::min)(std::strlen(name.c_str()),std::size_t{23});
            for(std::size_t i=0;i<24;++i)
                Require(actual[2+i]==(i<count?static_cast<u8>(name[i]):0),"pet rename bytes/termination/defined padding differ");
        }
        for(const int action:std::array<int,7>{{0,1,2,3,4,0x1234,-1}}){
            Require(mode.SendMsg(163,action,0,0)==1,"pet command sender failed");
            const auto actual=peer.ReceiveExact(3);
            Require(actual[0]==0xa1 && actual[1]==1 && actual[2]==static_cast<u8>(action),"pet command did not preserve source low byte");
        }
        Require(mode.SendMsg(158,0,0,0)==0,"null pet rename pointer accepted");
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"null pet name emitted a request");
        Require(g_session.m_petStatus.name==priorPet.name && g_session.m_petStatus.modified==priorPet.modified,
            "pet sender performed caller-owned optimistic state mutation");
    }
