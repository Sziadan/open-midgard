#include "ui/UIPetInfoWnd.h"
#include "ui/UIChatRoomSettingsWnd.h"
#include "ui/UIWindowMgr.h"
#include "ui/UIChatRoomEntry.h"
#include "ui/UiScale.h"
#include "main/WinMain.h"
#include "core/Globals.h"
#include "gamemode/Mode.h"
#include "qtui/QtUiState.h"
#include "qtui/QtUiStateAdapter.h"
#include "session/Session.h"
#include "res/Res.h"
#include "res/ActRes.h"
#include "res/Sprite.h"
#include <QBuffer>
#include <QColor>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickItem>
#include <QQuickView>
#include <QThread>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

void CheckPetWindowUi(QGuiApplication& app,QQuickView& view,QtUiState* state,
    CGameMode& mode,QtUiStateAdapter& adapter,const QString& output)
{
    Require(state!=nullptr,"pet Qt state is missing");
    InventoryTabViewportFixture viewport;viewport.Create("pet-window");
    {
        ArgbDibSurface surface;Require(surface.EnsureSize(4,4),"clip regression DIB failed");
        const auto reset=[&](){std::fill_n(surface.GetPixels(),16,0xff112233u);};
        std::array<u32,16> pixels{};
        for(size_t i=0;i<pixels.size();++i)pixels[i]=0xff556600u+static_cast<u32>(i);
        reset();int saved=SaveDC(surface.GetDC());IntersectClipRect(surface.GetDC(),1,1,3,3);
        Require(AlphaBlendArgbToHdc(surface.GetDC(),0,0,4,4,pixels.data(),4,4),"rectangular clip blend failed");GdiFlush();
        for(int y=0;y<4;++y)for(int x=0;x<4;++x)Require(surface.GetPixels()[y*4+x]==(x>=1 && x<3 && y>=1 && y<3?pixels[y*4+x]:0xff112233u),"DIB fast path ignored DC clip/source offset");
        RestoreDC(surface.GetDC(),saved);reset();saved=SaveDC(surface.GetDC());IntersectClipRect(surface.GetDC(),0,0,0,0);
        AlphaBlendArgbToHdc(surface.GetDC(),0,0,4,4,pixels.data(),4,4);GdiFlush();
        Require(std::all_of(surface.GetPixels(),surface.GetPixels()+16,[](u32 p){return p==0xff112233;}),"empty clip wrote DIB pixels");
        RestoreDC(surface.GetDC(),saved);reset();saved=SaveDC(surface.GetDC());
        const auto first=CreateRectRgn(0,0,2,2),second=CreateRectRgn(2,2,4,4),region=CreateRectRgn(0,0,0,0);
        CombineRgn(region,first,second,RGN_OR);SelectClipRgn(surface.GetDC(),region);DeleteObject(first);DeleteObject(second);DeleteObject(region);
        Require(AlphaBlendArgbToHdc(surface.GetDC(),0,0,4,4,pixels.data(),4,4),"complex clip GDI fallback failed");GdiFlush();
        for(int y=0;y<4;++y)for(int x=0;x<4;++x)Require((surface.GetPixels()[y*4+x]&0xffffff)==((x<2 && y<2) || (x>=2 && y>=2)?pixels[y*4+x]&0xffffff:0x112233u),"complex clip was reduced to bounding box");
        RestoreDC(surface.GetDC(),saved);reset();saved=SaveDC(surface.GetDC());SetViewportOrgEx(surface.GetDC(),1,0,nullptr);
        Require(AlphaBlendArgbToHdc(surface.GetDC(),0,0,2,2,pixels.data(),4,4,1,1,2,2),"mapped GDI fallback failed");GdiFlush();
        Require((surface.GetPixels()[0]&0xffffff)==0x112233 && (surface.GetPixels()[1]&0xffffff)==(pixels[5]&0xffffff) && (surface.GetPixels()[3]&0xffffff)==0x112233,"direct DIB path ignored viewport mapping");
        RestoreDC(surface.GetDC(),saved);
    }
    Require(!g_windowMgr.m_petInfoWnd && !g_windowMgr.m_roomMessageBox,
        "pet render fixture requires no existing pet owner or room modal");
    struct Restore {
        ro::pet::Status pet=g_session.m_petStatus;
        SubChatWindowInfo geometry=g_windowMgr.m_petInfoWindowInfo;
        int playerJob=g_session.m_playerJob,scale=GetConfiguredUiScalePercent();
        int folder=g_readFolderFirst,codePage=g_codePage;
        std::filesystem::path cwd=std::filesystem::current_path();
        ~Restore(){
            if(g_windowMgr.m_petInfoWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_petInfoWnd);
            if(g_windowMgr.m_roomMessageBox)g_windowMgr.DeleteWindow(g_windowMgr.m_roomMessageBox);
            g_session.m_petStatus=std::move(pet);g_session.m_playerJob=playerJob;
            g_windowMgr.m_petInfoWindowInfo=geometry;SetRuntimeUiScalePercent(scale);
            g_readFolderFirst=folder;g_codePage=codePage;std::filesystem::current_path(cwd);
        }
    } restore;

    const auto assets=std::filesystem::path(output.toStdString())/"pet-window-assets";
    std::filesystem::create_directories(assets/"source-bitmaps");
    const std::string encodedUi="\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA";
    const std::string rootKey="texture\\"+encodedUi;
    const std::string basicKey=rootKey+"\\basic_interface";
    const auto makeDirectory=[](std::string path){
#if RO_PLATFORM_WINDOWS
        Require(CreateDirectoryA(path.c_str(),nullptr) || GetLastError()==ERROR_ALREADY_EXISTS,
            "pet source-key directory creation failed");
#else
        std::replace(path.begin(),path.end(),'\\','/');std::filesystem::create_directories(path);
#endif
    };
    makeDirectory(assets.string()+"\\texture");
    makeDirectory(assets.string()+"\\"+rootKey);
    makeDirectory(assets.string()+"\\"+basicKey);
    makeDirectory(assets.string()+"\\"+rootKey+"\\illust");
    makeDirectory(assets.string()+"\\"+encodedUi);
    makeDirectory(assets.string()+"\\"+encodedUi+"\\illust");
    makeDirectory(assets.string()+"\\data");makeDirectory(assets.string()+"\\data\\texture");
    makeDirectory(assets.string()+"\\data\\texture\\"+encodedUi);
    makeDirectory(assets.string()+"\\data\\texture\\"+encodedUi+"\\illust");

    struct BitmapFixture {const char* name;int width,height;const char* color;bool basic;};
    const std::array<BitmapFixture,21> bitmaps{{
        {"titlebar_left.bmp",12,17,"#d24240",true},{"titlebar_right.bmp",12,17,"#4042d2",true},
        {"titlebar_mid.bmp",12,17,"#42d240",true},{"sysboxs_ld.bmp",8,8,"#d2d240",false},
        {"sysboxs_rd.bmp",8,8,"#42d2d2",false},
        {"sys_close_off.bmp",11,11,"#308040",true},{"sys_close_on.bmp",11,11,"#50b060",true},
        {"btn_rewrite.bmp",40,20,"#c040c0",false},{"btn_rewrite_a.bmp",40,20,"#40c0a0",false},
        {"btn_rewrite_b.bmp",40,20,"#c08040",false},
        {"txtbox_btn_a.bmp",13,18,"#c06030",true},{"txtbox_btn_b.bmp",13,18,"#30a060",true},
        {"txtbox_btn_c.bmp",13,18,"#6040c0",true},
        {"btn_ok.bmp",40,20,"#c0a080",false},{"btn_ok_a.bmp",40,20,"#80a0c0",false},
        {"btn_ok_b.bmp",40,20,"#806040",false},
        {"btn_cancel.bmp",40,20,"#a0c080",false},{"btn_cancel_a.bmp",40,20,"#80c0a0",false},
        {"btn_cancel_b.bmp",40,20,"#608040",false},
        {"sys_base_off.bmp",11,11,"#9c3030",true},{"sys_base_on.bmp",11,11,"#d05050",true}
    }};
    const auto saveBitmap=[&](const std::string& relative,const BitmapFixture& spec){
        QImage image(spec.width,spec.height,QImage::Format_RGB32);image.fill(QColor(spec.color));
        QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);
        Require(image.save(&buffer,"BMP"),"pet bitmap fixture encoding failed");
        const auto archive=assets/"source-bitmaps"/std::filesystem::path(relative);
        std::filesystem::create_directories(archive.parent_path());
        {std::ofstream file(archive,std::ios::binary);file.write(bytes.constData(),bytes.size());
            Require(file.good(),"pet bitmap fixture archive copy failed");}
        const std::string keyDir=spec.basic?basicKey:rootKey;
        std::string destination=assets.string()+"\\"+keyDir+"\\"+spec.name;
#if RO_PLATFORM_WINDOWS
        const auto file=CreateFileA(destination.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        Require(file!=INVALID_HANDLE_VALUE,"pet source-key bitmap open failed");DWORD count=0;
        const bool wrote=WriteFile(file,bytes.constData(),DWORD(bytes.size()),&count,nullptr)!=0;CloseHandle(file);
        Require(wrote && count==DWORD(bytes.size()),"pet source-key bitmap write failed");
#else
        std::replace(destination.begin(),destination.end(),'\\','/');
        std::ofstream file(destination,std::ios::binary);file.write(bytes.constData(),bytes.size());
        Require(file.good(),"pet source-key bitmap write failed");
#endif
    };
    for(const auto& spec:bitmaps)saveBitmap(spec.basic?std::string("basic_interface/")+spec.name:spec.name,spec);

    {
        std::array<std::string,1023> rows;rows.fill("Pet status");
        rows[584]="Pet Window";rows[407]="Name";rows[408]="Level";
        rows[585]="Hunger";rows[586]="Intimacy";rows[667]="Very hungry";rows[668]="Hungry";
        rows[669]="Normal";rows[670]="Replete";rows[671]="Very replete";rows[676]="Unknown";
        rows[672]="Very awkward";rows[673]="Awkward";rows[674]="Friendly";rows[675]="Very friendly";
        rows[1021]="Hate";rows[1022]="Very hate";rows[597]="Accessory";rows[598]="Equipped";rows[600]="Unequipped";
        rows[592]="Feed";rows[593]="Performance";rows[594]="Return egg";rows[595]="Accessory off";rows[596]="Pet Info";
        rows[601]="Feed this pet?";rows[682]="Name too long";rows[683]="Rename %s once?";
        std::ofstream messages(assets/"msgstringtable.txt",std::ios::binary);
        for(const auto& row:rows)messages<<row<<"#\r\n";
        Require(messages.good(),"pet message table fixture failed");
    }

    std::filesystem::current_path(assets);g_readFolderFirst=1;g_codePage=65001;
    SetRuntimeUiScalePercent(100);
    g_session.m_playerJob=4051;
    g_session.m_petStatus={};g_session.m_petStatus.valid=true;g_session.m_petStatus.name="Poring";
    g_session.m_petStatus.level=17;g_session.m_petStatus.fullness=10;g_session.m_petStatus.relationship=35;
    g_session.m_petStatus.job=1011;g_session.m_petStatus.accessoryItemId=0;g_session.m_petStatus.eggIndex=0;
    g_windowMgr.m_petInfoWindowInfo={150,100,280,200};

    const auto illustration=g_session.GetPetIllustName(g_session.m_petStatus.job,0);
    Require(illustration && *illustration,"pet illustration source key missing");
    BitmapFixture portraitSpec{"portrait.bmp",36,48,"#e020d0",false};
    QImage portraitImage(portraitSpec.width,portraitSpec.height,QImage::Format_RGB32);portraitImage.fill(QColor(portraitSpec.color));
    QByteArray portraitBytes;QBuffer portraitBuffer(&portraitBytes);portraitBuffer.open(QIODevice::WriteOnly);
    Require(portraitImage.save(&portraitBuffer,"BMP"),"pet portrait fixture encoding failed");
    {std::ofstream file(assets/"source-bitmaps"/"portrait.bmp",std::ios::binary);file.write(portraitBytes.constData(),portraitBytes.size());
        Require(file.good(),"pet portrait archive copy failed");}
    std::string portraitPath=assets.string()+"\\"+illustration;
#if RO_PLATFORM_WINDOWS
    const auto portraitFile=CreateFileA(portraitPath.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    Require(portraitFile!=INVALID_HANDLE_VALUE,"pet source-key portrait open failed");DWORD portraitCount=0;
    const bool portraitWrote=WriteFile(portraitFile,portraitBytes.constData(),DWORD(portraitBytes.size()),&portraitCount,nullptr)!=0;CloseHandle(portraitFile);
    Require(portraitWrote && portraitCount==DWORD(portraitBytes.size()),"pet source-key portrait write failed");
#else
    std::replace(portraitPath.begin(),portraitPath.end(),'\\','/');
    std::ofstream portraitFile(portraitPath,std::ios::binary);portraitFile.write(portraitBytes.constData(),portraitBytes.size());
    Require(portraitFile.good(),"pet source-key portrait write failed");
#endif

    auto* window=dynamic_cast<UIPetInfoWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_PETINFOWND));
    Require(window && window->m_show && window->m_w==280 && window->m_h==160
        && window->m_x==150 && window->m_y==100,"actual pet owner creation/default geometry differs");
    Require(window->GetFramePieces().size()>=5 && window->GetPortrait().bitmap.IsValid()
        && window->GetPortrait().bitmap.width==36 && window->GetNameEdit()
        && window->GetButtons()[0] && window->GetButtons()[1] && window->GetButtons()[2],
        "pet source-key frame, button or illustration artwork failed to load");
    Require(g_modeMgr.GetCurrentMode()!=nullptr,"pet input fixture requires a live game mode");

    const auto pointer=[&](UINT message,int x,int y){
        const WPARAM buttons=message==WM_LBUTTONDOWN || (message==WM_MOUSEMOVE && g_windowMgr.m_captureWindow)?MK_LBUTTON:0;
        if(message==WM_KEYDOWN)WindowProc(g_hMainWnd,message,static_cast<WPARAM>(x),0);
        else WindowProc(g_hMainWnd,message,buttons,MAKELPARAM(UiScaleLogicalToRawCoordinate(x),UiScaleLogicalToRawCoordinate(y)));
        adapter.syncChat();app.processEvents();
    };
    const auto click=[&](int x,int y){pointer(WM_LBUTTONDOWN,x,y);pointer(WM_LBUTTONUP,x,y);};
    int frames=0;
    const auto capture=[&](const QString& stem,bool modal){
        state->setUiScale(GetConfiguredUiScaleFactor());adapter.syncChat();
        view.requestUpdate();for(int i=0;i<6;++i){app.processEvents();QThread::msleep(20);}
        auto* owner=Find(view.rootObject(),"sourcePetInfo");
        auto* menuItem=Find(view.rootObject(),"sourcePetCommandMenu");
        Require(owner && owner->isVisible(),"actual QML pet owner missing");
        const double scale=GetConfiguredUiScaleFactor();const auto bounds=owner->mapRectToScene(owner->boundingRect());
        Require(std::abs(bounds.left()-window->m_x*scale)<0.01 && std::abs(bounds.top()-window->m_y*scale)<0.01
            && std::abs(bounds.width()-window->m_w*scale)<0.01 && std::abs(bounds.height()-window->m_h*scale)<0.01,
            "actual QML pet owner geometry differs from native owner");
        const QVariantMap data=state->chatWindowUi().value("petInfo").toMap();
        Require(data.value("visible").toBool() && data.value("width").toInt()==280 && data.value("height").toInt()==160
            && data.value("framePieces").toList().size()==static_cast<int>(window->GetFramePieces().size())
            && data.value("buttons").toList().size()==3 && data.value("texts").toList().size()==static_cast<int>(window->GetDisplayTexts().size()),
            "native pet component projection lost frame, buttons or text rows");
        const auto edit=data.value("edit").toMap();
        Require(edit.value("visible").toBool()==(window->GetNameEdit()->m_x>=window->m_x)
            && edit.value("width").toInt()==80 && edit.value("height").toInt()==16,
            "native/QML pet name editor visibility or geometry differs");
        const auto portrait=data.value("portrait").toMap();
        Require(portrait.value("width").toInt()==window->GetPortrait().bitmap.width
            && portrait.value("height").toInt()==window->GetPortrait().bitmap.height
            && !portrait.value("artwork").toString().isEmpty(),"native/QML pet portrait projection differs");
        Require(menuItem!=nullptr,"actual QML command-menu component missing");
        if(window->GetCommandMenu()){
            const QVariantMap menu=data.value("menu").toMap();const auto menuBounds=menuItem->mapRectToScene(menuItem->boundingRect());
            Require(menu.value("visible").toBool() && menu.value("rows").toList().size()==4
                && qRound(menuBounds.left())==UiScaleLogicalToRawCoordinate(window->GetCommandMenu()->m_x)
                && qRound(menuBounds.top())==UiScaleLogicalToRawCoordinate(window->GetCommandMenu()->m_y)
                && qRound(menuItem->width())==110 && qRound(menuItem->height())==64
                && menuItem->z()>owner->z(),"actual QML pet command menu geometry/layer differs");
        }else Require(!data.contains("menu") || !data.value("menu").toMap().value("visible").toBool(),
            "QML pet menu visible without native menu owner");

        ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"pet native DIB allocation failed");
        std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);
        g_windowMgr.DrawVisibleWindowsToHdc(dib.GetDC(),true);
        auto native=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy();
        if(GetConfiguredUiScalePercent()!=100)native=native.scaled(qRound(native.width()*scale),qRound(native.height()*scale),
            Qt::IgnoreAspectRatio,Qt::FastTransformation).copy(0,0,view.width(),view.height());
        const auto qml=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        Require(native.save(output+"/"+stem+".native.png") && qml.save(output+"/"+stem+".qml.png"),
            "pet paired native/QML frame save failed");
        const auto sample=[&](int x,int y){return QPoint(UiScaleLogicalToRawCoordinate(x),UiScaleLogicalToRawCoordinate(y));};
        const auto same=[&](QPoint p,const QColor& expected,const char* what){
            Require(native.rect().contains(p) && qml.rect().contains(p),"pet art sample outside viewport");
            if(native.pixelColor(p)!=expected || qml.pixelColor(p)!=expected)
                throw std::runtime_error((std::string("pet native/QML ")+what+" pixel mismatch native="
                    +native.pixelColor(p).name().toStdString()+" qml="+qml.pixelColor(p).name().toStdString()
                    +" expected="+expected.name().toStdString()).c_str());
        };
        same(sample(window->m_x+1,window->m_y+1),QColor("#d24240"),"title-left source art");
        same(sample(window->m_x+60,window->m_y+30),QColor("white"),"body fill");
        if(g_session.m_petStatus.job==1002){
            same(sample(window->m_x+5,window->m_y+87),QColor("#00ff00"),"fallback portrait left clamp");
            same(sample(window->m_x+5,window->m_y+91),QColor("white"),"fallback portrait flip");
            same(sample(window->m_x+5,window->m_y+95),QColor(127,255,127),"fallback portrait RGBA premultiplication");
            same(sample(window->m_x+5,window->m_y+162),QColor("#181b20"),"fallback portrait parent clip");
        }else same(sample(window->m_x+15,window->m_y+31),QColor("#e020d0"),"portrait source art");
        if(!edit.value("visible").toBool())same(sample(window->m_x-99,window->m_y-99),QColor("#181b20"),"hidden pet child parent clip");
        if(window->GetCommandMenu()){
            const auto& commandMenu=*window->GetCommandMenu();
            Require(commandMenu.GetSelection()==1,"pet menu hover selection was not projected");
            const QPoint selectedRow=sample(commandMenu.m_x+105,commandMenu.m_y+24);
            Require(native.pixelColor(selectedRow)==QColor("#32c8c8")
                && qml.pixelColor(selectedRow)==QColor("#32c8c8"),
                "pet command menu selected-row native/QML artwork sample differs");
        }
        if(modal){
            auto* dialog=Find(view.rootObject(),"sourceRoomValidation");
            Require(dialog && dialog->isVisible() && qRound(dialog->width())==280 && qRound(dialog->height())==120,
                "actual QML pet confirmation geometry differs");
            const auto& cancelButton=g_windowMgr.m_roomMessageBox->GetCancelButton();
            const QPoint buttonSample=sample(g_windowMgr.m_roomMessageBox->m_x+cancelButton.x+cancelButton.bitmaps[0].width/2,
                g_windowMgr.m_roomMessageBox->m_y+cancelButton.y+cancelButton.bitmaps[0].height/2);
            same(buttonSample,QColor("#a0c080"),"confirmation cancel source art");
        }
        if(!modal){
            const auto& close=window->GetButtons()[0]->GetButton();
            const QPoint closeSample=sample(window->GetButtons()[0]->m_x+5,window->GetButtons()[0]->m_y+5);
            const QColor closeColor=window->GetButtons()[0]->GetVisualState()==0?QColor("#308040"):QColor("#50b060");
            same(closeSample,closeColor,"close button artwork");
            Require(!close.uris[window->GetButtons()[0]->GetVisualState()].empty(),"pet close button source URI absent");
        }
        ++frames;
    };

    capture("pet-unmodified-editor",false);
    {
        const auto put16=[](std::vector<u8>& bytes,u16 value){bytes.push_back(static_cast<u8>(value));bytes.push_back(static_cast<u8>(value>>8));};
        const auto put32=[](std::vector<u8>& bytes,u32 value){for(int i=0;i<4;++i)bytes.push_back(static_cast<u8>(value>>(8*i)));};
        const std::string monster="\xB8\xF3\xBD\xBA\xC5\xCD";
        makeDirectory(assets.string()+"\\data\\sprite");makeDirectory(assets.string()+"\\data\\sprite\\"+monster);
        const std::string stem=assets.string()+"\\data\\sprite\\"+monster+"\\"+g_session.GetJobName(1002);
        const auto writeSource=[&](const std::string& file,const std::vector<u8>& bytes){
            const HANDLE handle=CreateFileA(file.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
            Require(handle!=INVALID_HANDLE_VALUE,"pet fallback source file open failed");DWORD count=0;
            const bool wrote=WriteFile(handle,bytes.data(),static_cast<DWORD>(bytes.size()),&count,nullptr)!=0;CloseHandle(handle);
            Require(wrote && count==bytes.size(),"pet fallback source file write failed");
        };
        std::vector<u8> spr{'S','P',1,2};put16(spr,2);put16(spr,1);
        put16(spr,100);put16(spr,2);put16(spr,200);for(int y=0;y<2;++y)for(int x=0;x<100;++x)spr.push_back(x?2:1);
        put16(spr,2);put16(spr,2);put16(spr,4);spr.insert(spr.end(),{1,2,1,2});
        put16(spr,2);put16(spr,2);for(int i=0;i<4;++i)spr.insert(spr.end(),{128,0,255,0});
        std::array<u8,1024> palette{};palette[5]=255;palette[8]=palette[9]=palette[10]=255;spr.insert(spr.end(),palette.begin(),palette.end());
        std::vector<u8> act(16);act[0]='A';act[1]='C';act[2]=5;act[3]=2;act[4]=1;
        put32(act,1);act.resize(act.size()+32,0);put32(act,4);
        const auto clip=[&](int x,int y,int index,int flags,int type,int width,int height){
            put32(act,static_cast<u32>(x));put32(act,static_cast<u32>(y));put32(act,index);put32(act,flags);
            act.insert(act.end(),{255,255,255,255});put32(act,0x3f800000);put32(act,0x3f800000);put32(act,0);put32(act,type);put32(act,width);put32(act,height);
        };
        clip(19,5,0,0,0,100,2);clip(-100,9,1,1,0,2,2);clip(-100,80,1,0,0,2,2);clip(-100,13,0,0,1,2,2);
        put32(act,0xffffffff);put32(act,0);put32(act,0);put32(act,0x40800000);
        writeSource(stem+".spr",spr);writeSource(stem+".act",act);
        g_resMgr.RegisterType("spr","data\\sprite\\",new CSprRes());g_resMgr.RegisterType("act","data\\sprite\\",new CActRes());
        g_session.m_petStatus.job=1002;const auto& fallback=window->GetPortrait().bitmap;
        Require(fallback.IsValid() && fallback.width==90 && fallback.height==155,"actual pet ACT/SPR fallback did not compose");
        Require(fallback.pixels[66*90]==0xff00ff00 && fallback.pixels[66*90+89]==0xffffffff
            && fallback.pixels[70*90]==0xffffffff && fallback.pixels[70*90+1]==0xff00ff00
            && fallback.pixels[65*90]==0 && fallback.pixels[74*90]==0x8000ff00 && fallback.pixels[141*90]==0xff00ff00,"pet fallback center/clamp/crop/flip/RGBA/layer positions differ");
        capture("pet-sprite-fallback",false);g_session.m_petStatus.job=1011;
    }
    const int wx=window->m_x,wy=window->m_y;
    click(window->GetButtons()[2]->m_x+6,window->GetButtons()[2]->m_y+8);
    auto* menu=window->GetCommandMenu();
    Require(menu && menu->m_show && menu->m_x==wx+160 && menu->m_y==wy+44,
        "actual manager input did not open source pet command menu");
    pointer(WM_MOUSEMOVE,menu->m_x+100,menu->m_y+24);
    Require(menu->GetSelection()==1,"native pet menu input did not hover second command");
    capture("pet-command-menu",false);
    pointer(WM_LBUTTONDOWN,menu->m_x+100,menu->m_y+24);
    Require(g_windowMgr.m_captureWindow==menu && GetCapture()==g_hMainWnd,"pet popup did not acquire native capture");
    WindowProc(g_hMainWnd,WM_CANCELMODE,0,0);
    Require(!g_windowMgr.m_captureWindow && GetCapture()!=g_hMainWnd && window->GetCommandMenu()==menu,"pet popup cancel mode retained capture or destroyed its owner");
    pointer(WM_LBUTTONUP,menu->m_x+300,menu->m_y+100);
    Require(!g_windowMgr.m_roomMessageBox && window->GetCommandMenu()==menu,"cancelled popup press activated a pet command");
    pointer(WM_KEYDOWN,VK_ESCAPE,0);
    Require(!window->GetCommandMenu(),"native Escape did not close pet command menu");

    click(window->GetButtons()[1]->m_x+10,window->GetButtons()[1]->m_y+10);
    auto* confirmation=g_windowMgr.m_roomMessageBox;
    Require(confirmation && confirmation->GetSourceOwner()==window && confirmation->GetMessageId()==683
        && confirmation->HasCancel() && confirmation->GetText()=="Rename Poring once?",
        "actual manager rename input did not create one-time confirmation");
    auto* modalItem=Find(view.rootObject(),"sourceRoomValidation");
    Require(modalItem && modalItem->isVisible() && modalItem->z()>Find(view.rootObject(),"sourcePetInfo")->z(),
        "actual Qt rename confirmation is missing or below pet owner");
    const auto modalData=state->chatWindowUi().value("roomMessage").toMap();
    Require(modalData.value("hasCancel").toBool() && modalData.value("messageId").toInt()==683
        && modalData.value("text").toString()=="Rename Poring once?","rename modal projection omitted cancel or formatted name");
    capture("pet-rename-confirmation",true);
    const auto& cancel=confirmation->GetCancelButton();
    const int cancelX=confirmation->m_x+cancel.x+cancel.bitmaps[0].width/2;
    const int cancelY=confirmation->m_y+cancel.y+cancel.bitmaps[0].height/2;
    pointer(WM_LBUTTONDOWN,cancelX,cancelY);
    Require(g_windowMgr.m_captureWindow==confirmation && GetCapture()==g_hMainWnd && confirmation->IsCancelPressed(),
        "native rename cancel press did not acquire capture");
    pointer(WM_LBUTTONUP,cancelX+100,cancelY+80);
    Require(g_windowMgr.m_roomMessageBox==confirmation && !confirmation->IsCancelPressed() && !GetCapture(),
        "outside release incorrectly accepted or dismissed pet rename confirmation");
    const int outX=confirmation->m_x+cancel.x+cancel.bitmaps[0].width/2;
    const int outY=confirmation->m_y+cancel.y+cancel.bitmaps[0].height/2;
    pointer(WM_LBUTTONDOWN,outX,outY);pointer(WM_LBUTTONUP,outX,outY);
    Require(!g_windowMgr.m_roomMessageBox && g_windowMgr.m_petInfoWnd==window
        && g_session.m_petStatus.name=="Poring" && !g_session.m_petStatus.modified,
        "native confirmation Cancel changed pet name or retained modal");
    capture("pet-rename-cancelled",false);

    g_windowMgr.DeleteWindow(window);
    Require(!g_windowMgr.m_petInfoWnd,"pet owner deletion retained manager reference");
    g_session.m_petStatus.name="Mimic";g_session.m_petStatus.modified=1;
    g_session.m_petStatus.fullness=91;g_session.m_petStatus.relationship=800;g_session.m_playerJob=4001;
    window=dynamic_cast<UIPetInfoWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_PETINFOWND));
    Require(window && window->GetNameEdit()->m_x==window->m_x-100 && window->GetButtons()[1]->m_x==window->m_x-100,
        "modified pet name did not hide rename editor and button");
    const auto modifiedText=window->GetDisplayTexts();
    const auto hungerText=std::find_if(modifiedText.begin(),modifiedText.end(),[](const auto& text){return text.x==145 && text.y==69;});
    const auto intimacyText=std::find_if(modifiedText.begin(),modifiedText.end(),[](const auto& text){return text.x==145 && text.y==89;});
    Require(modifiedText.size()>=8 && modifiedText[0].text=="Mimic" && modifiedText[0].color==0x00ff00
        && modifiedText[1].text=="Mimic" && modifiedText[1].color==0x1e1e1e
        && hungerText!=modifiedText.end() && hungerText->text=="Very replete"
        && intimacyText!=modifiedText.end() && intimacyText->text=="Friendly",
        "modified name or non-special-job intimacy/hunger text fixture differs");
    capture("pet-modified-name-status",false);
    SetRuntimeUiScalePercent(125);capture("pet-125-percent",false);

    const auto* closeButton=window->GetButtons()[0];const int closeX=closeButton->m_x+closeButton->m_w/2,closeY=closeButton->m_y+closeButton->m_h/2;
    pointer(WM_LBUTTONDOWN,closeX,closeY);
    Require(g_windowMgr.m_captureWindow==closeButton && GetCapture()==g_hMainWnd,"native pet close press did not acquire capture");
    pointer(WM_LBUTTONUP,-30,-30);
    Require(g_windowMgr.m_petInfoWnd==window && !g_windowMgr.m_captureWindow && !GetCapture(),
        "outside release closed pet owner or retained native close capture");
    pointer(WM_LBUTTONDOWN,closeX,closeY);pointer(WM_LBUTTONUP,closeX,closeY);
    Require(!g_windowMgr.m_petInfoWnd && !g_windowMgr.m_captureWindow && !GetCapture(),
        "actual manager close-button input retained pet window/capture");
    adapter.syncChat();Frames(app,view);
    Require(!Find(view.rootObject(),"sourcePetInfo")->isVisible(),"closed pet owner remains visible in Qt projection");

    // Deleting a pet owner while its one-time prompt is live must also delete the owned modal.
    SetRuntimeUiScalePercent(100);g_session.m_petStatus.modified=0;g_session.m_petStatus.name="Poring";
    window=dynamic_cast<UIPetInfoWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_PETINFOWND));
    Require(window,"pet owner could not be reopened for modal lifetime check");
    click(window->GetButtons()[1]->m_x+10,window->GetButtons()[1]->m_y+10);
    Require(g_windowMgr.m_roomMessageBox && g_windowMgr.m_roomMessageBox->GetSourceOwner()==window,
        "pet modal owner was not recorded");
    g_windowMgr.DeleteWindow(window);
    Require(!g_windowMgr.m_petInfoWnd && !g_windowMgr.m_roomMessageBox && !g_windowMgr.m_modalWindow,
        "deleting pet owner retained its confirmation modal");
    adapter.syncChat();Frames(app,view);
    Require(!g_windowMgr.m_itemInfoWnd,"summary render requires fresh shared item detail");
    auto* detail=static_cast<UIItemInfoWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMINFOWND));
    ro::pet::Status alternate;alternate.name="Summary";alternate.level=10012;alternate.fullness=25;
    alternate.relationship=750;alternate.modified=1;alternate.accessoryItemId=65535;
    detail->SetPetSummary(alternate);
    {
        ArgbDibSurface metrics;Require(metrics.EnsureSize(4,4),"UTF8 metrics DC failed");
        const int one=MeasureNpcSayDialogColoredTextHeight(metrics.GetDC(),1,u8"가",true);
        Require(one>0 && MeasureNpcSayDialogColoredTextHeight(metrics.GetDC(),1,u8"가나다",true)==3*one,
            "UTF8 summary long-word wrapping split a codepoint");
    }
    const auto captureSummary=[&](const QString& stem){
        state->setUiScale(GetConfiguredUiScaleFactor());
        Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,RenderBackendType::LegacyDirect3D7,-1,-1),"summary gameplay projection failed");
        Frames(app,view);
        auto* projected=Find(view.rootObject(),"itemInfoWindow");
        auto* viewer=Find(view.rootObject(),"itemInfoDescriptionViewport");
        UIItemInfoWnd::DisplayData display;
        Require(detail->GetDisplayDataForQt(&display) && state->itemInfoVisible() && projected && projected->isVisible()
            && viewer && qRound(viewer->width())==182 && qRound(viewer->height())==80,
            "alternate summary did not reuse actual native/passive Qt detail viewer");
        const auto data=state->itemInfoData();
        Require(data.value("description").toString()==QString::fromStdString(display.description)
            && display.description.find("Summary\n")==0 && display.detailLines.empty(),
            "alternate summary native/Qt content or row order differs");
        const double scale=GetConfiguredUiScaleFactor();const auto bounds=projected->mapRectToScene(projected->boundingRect());
        Require(std::abs(bounds.left()-detail->m_x*scale)<0.01 && std::abs(bounds.top()-detail->m_y*scale)<0.01
            && std::abs(bounds.width()-detail->m_w*scale)<0.01 && std::abs(bounds.height()-detail->m_h*scale)<0.01,
            "alternate summary scale/geometry differs");
        ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"summary native DIB failed");
        std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);
        g_windowMgr.DrawVisibleWindowsToHdc(dib.GetDC(),true);
        auto native=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy();
        if(GetConfiguredUiScalePercent()!=100)native=native.scaled(qRound(native.width()*scale),qRound(native.height()*scale),Qt::IgnoreAspectRatio,Qt::FastTransformation).copy(0,0,view.width(),view.height());
        const auto qml=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        Require(native.save(output+"/"+stem+".native.png") && qml.save(output+"/"+stem+".qml.png"),"summary capture failed");
        ++frames;
    };
    captureSummary("pet-summary-fresh");
    ITEM_INFO retained{};retained.SetItemId(501);retained.m_num=2;retained.m_isIdentified=1;
    detail->SetItemInfo(retained,0,0);Require(!detail->HasPetSummary(),"item selection did not clear summary marker");
    detail->SetPetSummary(alternate);captureSummary("pet-summary-retained-item");
    SetRuntimeUiScalePercent(125);captureSummary("pet-summary-125-percent");
    SetRuntimeUiScalePercent(100);g_windowMgr.DeleteWindow(detail);
    adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,RenderBackendType::LegacyDirect3D7,-1,-1);Frames(app,view);
    Require(!state->itemInfoVisible() && !Find(view.rootObject(),"itemInfoWindow")->isVisible(),"closed summary remains projected");
#include "PetContextMenuRenderChecks.inc"
    QFile results(output+"/pet-window-render-results.json");
    Require(results.open(QIODevice::WriteOnly),"pet render result file open failed");
    results.write(QJsonDocument(QJsonObject{{"frames",frames},{"synthetic_source_key_frame_button_portrait_bitmaps",true},
        {"native_manager_windowproc_input",true},{"paired_native_qml_artwork_samples",true},{"native_menu_cancel_mode",true},{"native_parent_clip_and_rgba",true},
        {"unmodified_editor_and_status",true},{"modified_name_and_status",true},{"command_menu_geometry_layer_rows",true},{"actual_act_spr_fallback",true},
        {"rename_confirmation_cancel_and_outside_release",true},{"owner_delete_closes_confirmation",true},
        {"scale125",true},{"default_geometry_280x160_at_150x100",true},
        {"qml_warnings_checked_by_runner",true},{"outcome","PASS"}}).toJson(QJsonDocument::Compact));
}
