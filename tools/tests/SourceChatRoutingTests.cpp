#include "core/SourceChatRouting.h"
#include <cstdlib>
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"chat route failure "<<__LINE__<<'\n';std::exit(1);}}while(false)
int main()
{
    using namespace ro::chat;
    const auto route=[](int kind,std::string text,int mode,bool battle,bool party,bool guild,SourceChatModifiers keys={}){
        return SourceChatRoute(kind,text,mode,battle,party,guild,keys);
    };
    CHECK(route(0,"hello",0,false,false,false)==6);
    CHECK(route(0,"hello",0,true,true,true)==271);
    CHECK(route(0,"%hello",0,true,true,true)==271);
    CHECK(route(0,"hello",0,false,true,true,{true,true})==72);
    CHECK(route(0,"$hello",0,false,false,false)==142);
    CHECK(route(0,"%hello",0,false,false,true)==6);
    CHECK(route(0,"hello",1,false,true,true)==72);
    CHECK(route(0,"hello",1,false,true,true,{true,false})==6);
    CHECK(route(0,"hello",1,false,true,true,{false,true})==72);
    CHECK(route(0,"hello",2,false,true,true,{true,false})==142);
    CHECK(route(0,"hello",2,false,true,true,{false,true})==6);
    CHECK(route(0,"hello",2,false,true,false,{true,true})==72);
    CHECK(route(50,"hello",0,true,false,false)==6);
    CHECK(route(50,"$hello",0,true,false,true)==6);
    CHECK(route(50,"hello",2,true,true,true,{true,true})==142);
    CHECK(route(50,"%hello",1,false,true,false)==72);
    CHECK(route(0,"%",0,true,true,true)==0);
    CHECK(route(50,"$",2,false,true,true)==0);
    std::string text="%%literal";CHECK(SourceChatRoute(0,text,0,false,true,false,{})==72 && text=="%literal");
    text=" $literal";CHECK(SourceChatRoute(0,text,0,false,false,true,{})==6 && text==" $literal");
    return 0;
}
