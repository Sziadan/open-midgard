#include "core/GuildState.h"
#include "qtui/GuildEmblemUpload.h"
#include "qtui/GuildEmblemPicker.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <atomic>
#include <QString>
#include <QVariantMap>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
using u8=unsigned char;using u16=unsigned short;using u32=std::uint32_t;
struct CRagConnection {
    std::vector<std::vector<u8>> sent;
    static CRagConnection* instance(){static CRagConnection connection;return &connection;}
    bool SendPacket(const char* p,int size){sent.emplace_back(p,p+size);return true;}
};
struct UIGuildWnd {
    int m_w=720,m_h=470,tab=0;bool shown=true;
    void SetShow(int value){shown=value;}void StoreInfo(){}void Move(int,int){}void Refresh();
} window;
struct {
    UIGuildWnd* m_guildWnd=&window;
    void ClampWindowToClient(int*,int*,int,int){}
    void PushChatEvent(const char*,u32,int){}
} g_windowMgr;
struct TestSkill {int SKID=10000,upgradable=1;};
struct {
    struct {u32 guildId=77;bool isMaster=true;} m_localGuild;
    u32 m_gid=202,m_aid=101;
    ro::guild::WindowState m_guildWindow;
    int points=1;std::vector<TestSkill> skills{{}};
    int GetGuildSkillPoint(){return points;}
    const auto& GetGuildSkillItems(){return skills;}
} g_session;
int emblemPickers=0;
void RequestGuildEmblemUpload(){++emblemPickers;}
#include "GuildRefresh.inc"
#include "GuildAction.inc"
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    QEventLoop loop; QTimer heartbeat; heartbeat.setInterval(10);
    std::atomic<int> ticks{0};bool completed=false;
    QObject::connect(&heartbeat,&QTimer::timeout,[&]{++ticks;});
    heartbeat.start();
    assert(ro::guild::PickEmblemAsync(&app,[&]{
        assert(QThread::currentThread()!=app.thread());
        // Simulate browsing until the main loop has pumped eight heartbeats.
        for(int i=0;i<2000 && ticks<8;++i)QThread::msleep(1);
        return ro::guild::EmblemSelection{QStringLiteral("test.bmp"),false};
    },[&](const ro::guild::EmblemSelection& result){
        assert(QThread::currentThread()==app.thread());
        assert(result.path=="test.bmp" && !result.failed && ticks>=8);
        completed=true;loop.quit();
    }));
    assert(!ro::guild::PickEmblemAsync(&app,[]{return ro::guild::EmblemSelection{};},[](const auto&){}));
    QTimer::singleShot(5000,&loop,&QEventLoop::quit);loop.exec();assert(completed);
    heartbeat.stop();
    // Cancellation is delivered without fabricating a chosen file.
    completed=false;
    assert(ro::guild::PickEmblemAsync(&app,[]{return ro::guild::EmblemSelection{};},[&](const auto& result){
        assert(result.path.isEmpty() && !result.failed);completed=true;loop.quit();
    }));
    loop.exec();assert(completed);
    std::cout<<"PASS: asynchronous emblem picker keeps main-loop heartbeats running, completes on the game thread, rejects duplicate pickers and handles cancellation\n";
    QImage keyTest(5,1,QImage::Format_ARGB32);
    keyTest.setPixel(0,0,qRgb(255,0,255));keyTest.setPixel(1,0,qRgb(254,2,253));
    keyTest.setPixel(2,0,qRgb(245,168,223));keyTest.setPixel(3,0,qRgb(239,0,255));keyTest.setPixel(4,0,qRgb(255,16,255));
    const auto keyed=ro::guild::TransparentEmblemImage(keyTest);
    assert(qAlpha(keyed.pixel(0,0))==0 && qAlpha(keyed.pixel(1,0))==0);
    for(int x=2;x<5;++x)assert(keyed.pixel(x,0)==keyTest.pixel(x,0));
    if(argc>1){
        QImage sample(QString::fromLocal8Bit(argv[1]));assert(sample.size()==QSize(24,24));
        const auto cleaned=ro::guild::TransparentEmblemImage(sample);int removed=0;
        for(int y=0;y<24;++y)for(int x=0;x<24;++x){
            if(ro::guild::IsEmblemTransparencyKey(sample.pixel(x,y))){assert(qAlpha(cleaned.pixel(x,y))==0);++removed;}
            else assert(cleaned.pixelColor(x,y)==sample.pixelColor(x,y));
        }
        assert(removed>0);std::cout<<"PASS: supplied emblem transparency removes "<<removed<<" key pixels and preserves artwork\n";
    }
    QString error;
    assert(ro::guild::EmblemUploadPacket(QImage(),error).isEmpty() && !error.isEmpty());
    assert(ro::guild::EmblemUploadPacket(QImage(25,24,QImage::Format_RGB32),error).isEmpty());
    QImage image(24,24,QImage::Format_ARGB32); image.fill(Qt::transparent);
    image.setPixelColor(0,0,QColor(12,34,56));
    image.setPixelColor(1,0,QColor(254,2,253));
    const QByteArray upload=ro::guild::EmblemUploadPacket(image,error);
    assert(error.isEmpty() && upload.size()>4 && static_cast<unsigned char>(upload[0])==0x53 && upload[1]==1);
    assert((static_cast<unsigned char>(upload[2]) | (static_cast<unsigned char>(upload[3])<<8))==upload.size());
    // qUncompress expects the size prefix that is deliberately absent on the wire.
    const QByteArray bmp=qUncompress(QByteArray::fromHex("000006f6")+upload.mid(4));
    assert(bmp.size()==1782 && bmp.startsWith("BM") && ro::guild::Read(reinterpret_cast<const u8*>(bmp.constData())+2)==1782);
    assert(bmp[10]==54 && bmp[18]==24 && bmp[22]==24 && bmp[28]==24);
    const int topLeft=54+23*24*3;
    assert(bmp[topLeft]==56 && bmp[topLeft+1]==34 && bmp[topLeft+2]==12);
    assert(static_cast<unsigned char>(bmp[topLeft+3])==255 && bmp[topLeft+4]==0 && static_cast<unsigned char>(bmp[topLeft+5])==255);
    assert(static_cast<unsigned char>(bmp[54])==255 && bmp[55]==0 && static_cast<unsigned char>(bmp[56])==255);
    auto& sent=CRagConnection::instance()->sent;
    GuildAction("emblem",{});assert(emblemPickers==1 && sent.empty());
    window.Refresh();assert(sent.size()==6 && sent[0]==std::vector<u8>({0x4d,1}));
    for(int i=0;i<5;++i)assert(sent[i+1]==std::vector<u8>({0x4f,1,static_cast<u8>(i),0,0,0}));
    sent.clear();g_session.m_localGuild.guildId=0;window.Refresh();assert(sent.size()==1);g_session.m_localGuild.guildId=77;sent.clear();
    GuildAction("notice",{{"subject","Meeting"},{"body","Tonight in Prontera"}});
    assert(sent.back().size()==186 && sent.back()[0]==0x6e && sent.back()[1]==1 && ro::guild::Read(sent.back().data()+2)==77);
    assert(ro::guild::Text(sent.back().data()+6,60)=="Meeting"&&ro::guild::Text(sent.back().data()+66,120)=="Tonight in Prontera");
    GuildAction("position",{{"id",2},{"name","Officer"},{"tax",25},{"invite",true},{"expel",true}});
    const auto& p=sent.back();assert(p.size()==44&&p[0]==0x61&&p[2]==44&&ro::guild::Read(p.data()+4)==2&&ro::guild::Read(p.data()+8)==17&&ro::guild::Read(p.data()+16)==25&&ro::guild::Text(p.data()+20,24)=="Officer");
    const auto count=sent.size();GuildAction("position",{{"id",20}});GuildAction("notice",{{"subject",QString(60,'x')}});assert(sent.size()==count);
    GuildAction("skill",{{"id",10000}});assert(sent.back()==std::vector<u8>({0x12,1,0x10,0x27}));
    g_session.m_guildWindow.members.push_back({101,202,0,0,0,0,true,"Player"});
    GuildAction("memberPosition",{{"character",202},{"position",3}});
    assert(sent.back().size()==16&&sent.back()[0]==0x55&&ro::guild::Read(sent.back().data()+4)==101&&ro::guild::Read(sent.back().data()+8)==202&&ro::guild::Read(sent.back().data()+12)==3);
    g_session.m_localGuild.isMaster=false;const auto before=sent.size();
    GuildAction("emblem",{});assert(emblemPickers==1);
    GuildAction("notice",{{"subject","Unauthorized"}});GuildAction("position",{{"id",1}});GuildAction("skill",{{"id",10000}});GuildAction("memberPosition",{{"character",202},{"position",3}});
    GuildAction("leave",{{"confirmed",false}});assert(sent.size()==before);
    GuildAction("leave",{{"confirmed",true},{"reason","Goodbye"}});
    assert(sent.back().size()==54&&sent.back()[0]==0x59&&ro::guild::Read(sent.back().data()+2)==77&&ro::guild::Read(sent.back().data()+6)==101&&ro::guild::Read(sent.back().data()+10)==202&&ro::guild::Text(sent.back().data()+14,40)=="Goodbye");
    GuildAction("tab",{{"tab",99}});assert(window.tab==5);GuildAction("close",{});assert(!window.shown);
    std::cout<<"PASS: production guild requests and edits match Ref/rAthena wire layouts; role guards, field limits, skill gates and leave confirmation hold\n";
}
