#include "core/SourceChatParser.h"
#include "core/SourceChatAliases.h"
#include "core/UiMessageStrings.h"
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>

namespace {
void Require(bool value,int line){if(!value){std::cerr<<"Source parser failure at "<<line<<'\n';std::exit(1);}}
#define CHECK(value) Require((value),__LINE__)
std::map<int,std::string> messages;
ro::chat::SourceChatParseContext context;
auto Parse(std::string_view text){return ro::chat::ParseSourceChat(text,context,[](std::string_view command){
    return ro::chat::LookupSourceTalkType(command,[](int id)->std::optional<std::string>{
        const auto found=messages.find(id);if(found==messages.end())return std::nullopt;return found->second;
    },context.encoding);
});}
void CaseAt(int line,std::string_view text,int type,std::ptrdiff_t offset,std::string p0={},std::string p1={},std::string p2={})
{
    const auto parsed=Parse(text);if(parsed.type!=type || parsed.messageOffset!=offset)std::cerr<<"Case at "<<line<<" type "<<parsed.type<<"/"<<type<<" offset "<<parsed.messageOffset<<"/"<<offset<<"\n";
    CHECK(parsed.type==type);CHECK(parsed.messageOffset==offset);
    CHECK(parsed.parameters[0]==p0);CHECK(parsed.parameters[1]==p1);CHECK(parsed.parameters[2]==p2);
}
}
#define Case(...) CaseAt(__LINE__,__VA_ARGS__)
int main()
{
    using ro::chat::SourceChatEncoding;
    Case("",3,-1);Case(" \t\r\n",3,-1);Case("//comment",3,-1);
    Case("normal  trailing ",0,0);Case(" /am",0,0);Case("\"/am\"",0,0);
    Case("/AM ignored arguments",152,-1);Case("/automsg\"/sit\"",22,-1);
    Case("/am//discard pending",3,-1);Case("/am //comment",152,-1);
    Case("/b    message ",1,6);Case("/b\tmessage",1,2);Case("/kill victim",4,6);
    Case("/lb local",87,4);Case("/nb hide",90,4);Case("/nlb local",91,5);
    Case("/bb blue",102,4);Case("/mb localblue",103,4);
    // Tail offsets are fixed source byte offsets; they do not follow token spans.
    Case("/ignored\"/b\" tail",1,2);
    Case("/gc  \"Name With Spaces\"",57,-1," \"Name With Spaces\"");
    Case("/guildchat\tmessage",57,-1,"message");
    Case("/ex    Someone",18,-1,"   Someone");Case("/in \"Someone\"",19,-1,"\"Someone\"");
    Case("/ex",15,-1);Case("/exall extra",16,-1);Case("/inall",17,-1);
    context.soundVolume=73;context.musicVolume=44;
    Case("/v",12,-1,"73");Case("/bv",13,-1,"44");Case("/v 42 extra",12,-1,"42");
    Case("/red \"Mixed CASE\" x",20,-1,"Mixed CASE x ");Case("/red",20,-1);
    Case("/deal \"Other Player\"",25,-1,"Other Player");Case("/deal one extra",3,-1);
    Case("/guild \"Mixed Guild\"",55,-1,"Mixed Guild");Case("/guild",3,-1);Case("/guild one two",3,-1);
    Case("/breakguild \"Mixed Guild\"",56,-1,"Mixed Guild");Case("/organize Party",26,-1,"Party");
    Case("/item 501",48,-1,"501");Case("/monster Poring",48,-1,"Poring");
    Case("/mapmove prontera",49,-1,"prontera","0","0");
    Case("/mm prontera 1",49,-1,"prontera","0","0");
    Case("/mm prontera 1 2",49,-1,"prontera","1","2");
    Case("/mm prontera 1 2 extra",49,-1,"prontera","0","0");
    Case("/expel Player",28,-1,"Player");Case("/sm 6",58,-1,"6");
    Case("/resetstate whatever",61,-1);Case("/resetskill",62,-1);
    Case("/changemaptype map 1 2",63,-1,"map","1","2");Case("/cmt map",3,-1);
    Case("/emblem ignored",64,-1);Case("/hide",88,-1);Case("/font",92,-1);
    Case("/remove Name",95,-1,"Name");Case("/summon Name",98,-1,"Name");
    Case("/shift",96,-1);Case("/shift Name 1 2",96,-1,"Name","1","2");
    CHECK(Parse("/shift Name 1 2 extra").parameterOverflow);
    Case("/recall Name",97,-1,"Name");Case("/check Name",142,-1,"Name");Case("/rc Name",143,-1,"Name");
    Case("/showname extra",107,0);Case("/invite \"Friend Name\"",183,-1,"Friend Name");
    Case("/not-a-command",3,-1);Case("/am \"a // quoted tail\"",152,-1);
    const auto hi=Parse("/hi \"Mixed Name\" hello");CHECK(hi.type==150 && hi.parameters[0]=="Mixed Name");CHECK(hi.messageOverride=="Mixed Name hello ");
    CHECK(!Parse("/hi").messageOverride);
    for(const auto test: {std::pair<const char*,int>{"/str+ 257",0},{"/agi+ -1",1},{"/luk+ 9",2},{"/int+ 4",3},{"/vit+ 5",4},{"/dex+ 6",5}}){
        const auto p=Parse(test.first);CHECK(p.type==136 && p.messageOffset==0 && p.statIndex==test.second);
    }
    CHECK(ro::chat::SourceChatAtoi("2147483648")==INT32_MIN);
    CHECK(ro::chat::SourceChatAtoi("  -12trailing")==-12);
    CHECK(Parse("/str+ 257").statValue==1);CHECK(Parse("/agi+ -1").statValue==255);
    CHECK(ro::chat::SourceChatAtoi("99999999999999999999")==1661992959);
    CHECK(ro::chat::SourceChatAtoi("-99999999999999999999")==-1661992959);
    CHECK(Parse("/mineffect").toggleMinEffect);context.minEffectAlias="/local-effect";CHECK(Parse("/local-effect").toggleMinEffect);
    context.minEffectAlias="/deal";CHECK(Parse("/deal").toggleMinEffect);
    context.minEffectAlias="/remove";CHECK(Parse("/remove target").toggleMinEffect);
    context.minEffectAlias=u8"/폰트";CHECK(!Parse(u8"/폰트").toggleMinEffect); // Font branch precedes dynamic effect alias.
    context.minEffectAlias="";CHECK(Parse("/discard\"\"").toggleMinEffect);context.minEffectAlias.reset();
    // UTF-8 UI aliases adapt CP949 prefix lengths; the raw source form retains them.
    Case(u8"/ㅠ hello",1,5);context.encoding=SourceChatEncoding::LegacyCp949;
    Case(std::string("/\xA4\xD0 hello"),1,4);Case(u8"/ㅠ hello",3,-1);
    CHECK(ro::chat::SourceChatLower(std::string("\x81" "AB"),SourceChatEncoding::LegacyCp949)==std::string("\x81" "Ab"));
    context.encoding=SourceChatEncoding::Utf8;Case(u8"/자동응답",152,-1);
    const auto missing=[](int)->std::optional<std::string>{return std::nullopt;};
    CHECK(ro::chat::LookupSourceTalkType("/noshift",missing)!=3);
    CHECK(ro::chat::LookupSourceTalkType("/noshift",missing,SourceChatEncoding::Utf8,true)==3);
    CHECK(ro::chat::LookupSourceTalkType("/noctrl",missing,SourceChatEncoding::Utf8,true)==3);
    // Independent selected-PE checks on both early and late registrations.
    Case("/help",14,-1);Case("/savechat",51,-1);Case("/autosavechat",104,-1);Case("/eqopen",184,-1);
    // Resource-backed duplicate aliases preserve the first original registration.
    const auto& rules=ro::chat::kSourceNoParamRules;int first=-1,second=-1,firstType=-1;
    for(const auto& r:rules)if(r.messageId>=0){if(first<0){first=r.messageId;firstType=r.type;}else if(r.type!=firstType){second=r.messageId;break;}}
    CHECK(first>=0 && second>=0);messages[first]="/local-alias";messages[second]="/local-alias";
    Case("/local-alias",firstType,-1);messages[first]="";Case("/reset\"\"",firstType,-1);
    messages[first]="/AM";CHECK(Parse("/AM").type==152); // Alias tables are not lowercased.
    ro::UiMessageStrings table;table.Parse("zero##two#");CHECK(table.Find(1) && table.Find(1)->empty());CHECK(!table.Find(3));
    return 0;
}
