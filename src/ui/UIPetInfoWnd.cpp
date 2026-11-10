#include "UIPetInfoWnd.h"
#include "UIWindowMgr.h"
#include "UIItemWnd.h"
#include "core/Globals.h"
#include "core/File.h"
#include "core/UiMessageStrings.h"
#include "core/SettingsIni.h"
#include "gamemode/Mode.h"
#include "session/Session.h"
#include "res/Res.h"
#include "res/ActRes.h"
#include "res/Sprite.h"
#include <algorithm>
#include <filesystem>
#include <tuple>
#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#endif

namespace {
std::string Label(int id){
    static ro::UiMessageStrings table;
    static decltype(std::make_tuple(g_serviceType,g_codePage,g_readFolderFirst,std::filesystem::current_path())) cached;
    static bool loaded=false;
    const auto key=std::make_tuple(g_serviceType,g_codePage,g_readFolderFirst,std::filesystem::current_path());
    if(!loaded || key!=cached){table.Load();cached=key;loaded=true;}return table.Get(id,"");
}
std::string Utf8(const std::string& text)
{
    if(text.empty() || g_codePage==65001)return text;
#if RO_PLATFORM_WINDOWS
    const UINT page=g_codePage>0?static_cast<UINT>(g_codePage):CP_ACP;
    const int n=MultiByteToWideChar(page,0,text.data(),static_cast<int>(text.size()),nullptr,0);
    if(n<=0)return text;std::wstring wide(n,L'\0');MultiByteToWideChar(page,0,text.data(),static_cast<int>(text.size()),wide.data(),n);
    const int size=WideCharToMultiByte(CP_UTF8,0,wide.data(),n,nullptr,0,nullptr,nullptr);std::string out(size,'\0');
    if(size)WideCharToMultiByte(CP_UTF8,0,wide.data(),n,out.data(),size,nullptr,nullptr);return out;
#else
    return text;
#endif
}
void Text(HDC dc,int x,int y,int width,const std::string& text,u32 color)
{
    if(width<=0)return;
#if RO_ENABLE_QT6_UI
    QImage image(width,18,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
    QPainter p(&image);QFont font("Segoe UI");font.setPixelSize(12);p.setFont(font);p.setPen(QColor::fromRgb(color));
    p.drawText(image.rect(),Qt::AlignLeft|Qt::AlignTop|Qt::TextSingleLine,QString::fromUtf8(text.c_str()));p.end();
    AlphaBlendArgbToHdc(dc,x,y,width,18,reinterpret_cast<const unsigned int*>(image.constBits()),width,18);
#else
    shopui::DrawWindowTextRect(dc,{x,y,x+width,y+18},text,RGB((color>>16)&255,(color>>8)&255,color&255),DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX);
#endif
}
int Width(const std::string& text){
#if RO_ENABLE_QT6_UI
    QFont font("Segoe UI");font.setPixelSize(12);return QFontMetrics(font).horizontalAdvance(QString::fromUtf8(text.c_str()));
#else
    return static_cast<int>(text.size())*6;
#endif
}
std::string FormatName(const std::string& format,const std::string& name)
{
    std::string out;for(std::size_t i=0;i<format.size();++i){
        if(format[i]=='%' && i+1<format.size() && format[i+1]=='s'){out+=name;++i;}
        else if(format[i]=='%' && i+1<format.size() && format[i+1]=='%'){out+='%';++i;}
        else out+=format[i];
    }return out;
}
class PetNameEdit final:public UIEditCtrl {
public:
    void OnKeyDown(int key) override {if(key==VK_RETURN || key==VK_ESCAPE){if(m_parent)m_parent->OnKeyDown(key);}}
    void OnChar(char c) override {
        if(c=='\b'){
            std::size_t at=0,last=0;while(at<m_text.size()){
                last=at;
                if(g_codePage==65001){++at;while(at<m_text.size() && (static_cast<u8>(m_text[at])&0xc0)==0x80)++at;}
                else {
#if RO_PLATFORM_WINDOWS
                    at=static_cast<std::size_t>(CharNextExA(static_cast<WORD>(g_codePage),m_text.c_str()+at,0)-m_text.c_str());
#else
                    ++at;
#endif
                }
            }
            if(!m_text.empty()){m_text.resize(last);Invalidate();}
        }else UIEditCtrl::OnChar(c);
    }
    void OnDraw() override {
        auto dc=AcquireDrawTarget();if(!dc)return;
        shopui::FillRectColor(dc,{m_x,m_y,m_x+m_w,m_y+m_h},RGB(232,232,232));
        shopui::FrameRectColor(dc,{m_x,m_y,m_x+m_w,m_y+m_h},RGB(128,128,128));
        const int saved=SaveDC(dc);IntersectClipRect(dc,m_x+1,m_y+1,m_x+m_w-1,m_y+m_h-1);
        const auto display=Utf8(m_text);Text(dc,m_x+1,m_y+1,m_w-2,display,0);
        if(g_windowMgr.m_editWindow==this){const int x=m_x+1+Width(display);shopui::FillRectColor(dc,{x,m_y+2,x+1,m_y+m_h-2},RGB(0,0,0));}
        RestoreDC(dc,saved);ReleaseDrawTarget(dc);
    }
};
class PetMenuHit final:public UIButton {
public:
    void OnLBtnDown(int,int)override{g_windowMgr.m_captureWindow=this;}
    void OnLBtnUp(int x,int y)override{if(g_windowMgr.m_captureWindow==this && x>=m_x && x<m_x+m_w && y>=m_y && y<m_y+m_h && m_parent)m_parent->SendMsg(this,6,202,0,0);}
    void OnDraw()override{}
};
}

UIPetInfoWnd::UIPetInfoWnd()
{
    m_id=88;m_defPushId=m_defCancelPushId=117;Create(280,160);
    m_buttons[0]=new UIFriendOptionButton(135,"basic_interface\\sys_close_off.bmp","basic_interface\\sys_close_on.bmp","basic_interface\\sys_close_on.bmp",11,11);
    m_buttons[0]->Move(m_w-14,3);m_buttons[0]->SetToolTip("Close");AddChild(m_buttons[0]);
    m_nameEdit=new PetNameEdit();m_nameEdit->Create(80,16);m_nameEdit->m_maxchar=50;m_nameEdit->SetFrameColor(232,232,232);
    auto& pet=g_session.m_petStatus;
    if(pet.job!=-1 && pet.name.empty())pet.name=g_session.GetJobName(pet.job);
    m_nameEdit->SetText(pet.name.c_str());m_nameEdit->Move(pet.job==-1 || pet.modified?-100:135,pet.job==-1 || pet.modified?-100:24);AddChild(m_nameEdit);
    m_buttons[1]=new UIFriendOptionButton(118,"btn_rewrite.bmp","btn_rewrite_a.bmp","btn_rewrite_b.bmp",40,20);
    m_buttons[1]->Move(pet.modified?-100:230,pet.modified?-100:21);AddChild(m_buttons[1]);
    auto* menuHit=new PetMenuHit();menuHit->Create(120,18);menuHit->Move(150,44);menuHit->m_id=202;AddChild(menuHit);
    m_buttons[2]=new UIFriendOptionButton(202,"basic_interface\\txtbox_btn_a.bmp","basic_interface\\txtbox_btn_b.bmp","basic_interface\\txtbox_btn_c.bmp",13,18);
    m_buttons[2]->Move(270-m_buttons[2]->m_w,44);m_buttons[2]->SetToolTip("Pet Command");AddChild(m_buttons[2]);
    const char* pieces[]={"basic_interface\\titlebar_left.bmp","basic_interface\\titlebar_right.bmp","basic_interface\\titlebar_mid.bmp","sysboxs_ld.bmp","sysboxs_rd.bmp"};
    for(int i=0;i<5;++i){m_frame[i]=LoadChatRoomSkinBitmap(pieces[i],false);m_frameUris[i]=ChatRoomBitmapUri(m_frame[i]);}
}
UIPetInfoWnd::~UIPetInfoWnd(){CloseCommandMenu();}
void UIPetInfoWnd::LoadStoredInfo(SubChatWindowInfo& info)
{
    if(!LoadUiWindowPlacement("PetInfoWnd",&info.x,&info.y))return;
    info.width=LoadSettingsIniInt("UIWindows","PetInfoWndWidth",280);
    info.height=LoadSettingsIniInt("UIWindows","PetInfoWndHeight",160);
}
void UIPetInfoWnd::Move(int x,int y){const int dx=x-m_x,dy=y-m_y;UIWindow::Move(x,y);for(auto* child:m_children)child->Move(child->m_x+dx,child->m_y+dy);}
void UIPetInfoWnd::StoreInfo(){if(m_info)*m_info={m_x,m_y,m_w,m_h};SaveUiWindowPlacement("PetInfoWnd",m_x,m_y);SaveSettingsIniInt("UIWindows","PetInfoWndWidth",m_w);SaveSettingsIniInt("UIWindows","PetInfoWndHeight",m_h);}
void UIPetInfoWnd::CancelPointerCapture(){m_isDragging=0;for(auto* button:m_buttons)button->CancelPointerCapture();if(m_menu && g_windowMgr.m_captureWindow==m_menu)g_windowMgr.m_captureWindow=nullptr;for(auto* window=g_windowMgr.m_captureWindow;window;window=window->m_parent)if(window==this){g_windowMgr.m_captureWindow=nullptr;break;}}
void UIPetInfoWnd::OnLBtnDown(int x,int y){if(y-m_y<17)UIFrameWnd::OnLBtnDown(x,y);}
void UIPetInfoWnd::OnKeyDown(int key){if(key==VK_RETURN || key==VK_ESCAPE)SendMsg(this,key==VK_RETURN?0:1,0,0,0);}
std::string UIPetInfoWnd::GetEditDisplayText()const{return Utf8(m_nameEdit->GetText());}
void UIPetInfoWnd::RefreshReceivedName(){m_nameEdit->SetText(g_session.m_petStatus.name.c_str());Invalidate();}
std::vector<PetInfoText> UIPetInfoWnd::GetDisplayTexts()const
{
    const auto& pet=g_session.m_petStatus;std::vector<PetInfoText> out;
    const auto label=[&](int id,int x,int y){out.push_back({x,y,Utf8(Label(id)),0});};
    if(pet.modified){out.push_back({135,24,Utf8(pet.name),0x00ff00});out.push_back({136,24,Utf8(pet.name),0x1e1e1e});}
    label(584,5,2);label(407,100,24);label(408,100,44);label(585,100,69);label(586,100,89);
    const int h=pet.fullness;const int hungry=h<0 || h>100?676:h<=10?667:h<=25?668:h<=75?669:h<=90?670:671;
    const int f=pet.relationship;const bool special=g_session.m_playerJob>=4050 && g_session.m_playerJob<=4052;
    const int friendly=f<0 || f>1000?676:f<=100?(special && f<=40?(f<=20?1022:1021):672):f<=250?673:f<=750?669:f<=900?674:675;
    out.push_back({135,44,std::to_string(pet.level),0});label(hungry,145,69);label(friendly,145,89);
    if(pet.job>=1000 && pet.job<1986){
        const std::string command="< Command >";out.push_back({206-Width(command)/2,48,command,0});
        label(597,100,119);label(pet.accessoryItemId?598:600,100,139);
    }
    return out;
}
std::vector<PetInfoArtwork> UIPetInfoWnd::GetFramePieces()const
{
    std::vector<PetInfoArtwork> out;const auto append=[&](int i,int x,int y){if(m_frame[i].IsValid())out.push_back({x,y,m_frame[i],m_frameUris[i],i>=3?1:0});};
    append(0,0,0);append(1,m_w-m_frame[1].width,0);for(int x=12;x<m_w-12;x+=12)append(2,x,0);
    append(3,0,m_h-m_frame[3].height);append(4,m_w-m_frame[4].width,m_h-m_frame[4].height);return out;
}
const PetInfoArtwork& UIPetInfoWnd::GetPortrait()
{
    const auto& pet=g_session.m_petStatus;
    const char* illustration=g_session.GetPetIllustName(pet.job,pet.accessoryItemId);
    const bool illustrated=pet.job>=1000 && pet.job<1986 && g_fileMgr.IsDataExist(illustration);
    if(m_portraitJob==pet.job && m_portraitIllustrated==illustrated && m_portrait.bitmap.IsValid())return m_portrait;
    m_portraitIllustrated=illustrated;
    m_portraitJob=pet.job;m_portrait={};m_nativePortraitPixels.clear();m_portrait.x=5;m_portrait.y=21;
    if(pet.job<1000 || pet.job>=1986)return m_portrait;
    if(illustrated){
        m_portrait.bitmap=shopui::LoadBitmapPixelsFromGameData(illustration,false);
    }else{
        const std::string stem=std::string("\xB8\xF3\xBD\xBA\xC5\xCD\\")+g_session.GetJobName(pet.job);
        auto* act=g_resMgr.GetAs<CActRes>((stem+".act").c_str());
        auto* spr=g_resMgr.GetAs<CSprRes>((stem+".spr").c_str());
        const auto* motion=act?act->GetMotion(0,0):nullptr;
        if(motion && spr){
            auto& bitmap=m_portrait.bitmap;bitmap.width=90;bitmap.height=155;bitmap.pixels.assign(90*155,0);
            int alignX=0,alignY=0,startX=0,startY=0;
            for(std::size_t i=0;i<motion->sprClips.size();++i){
                const auto& clip=motion->sprClips[i];const auto* image=spr->GetSprite(clip.clipType,clip.sprIndex);
                if(!image)continue;
                int x,y;if(i==0){alignX=-clip.x;alignY=-clip.y;startX=(90-image->width)/2;startY=(134-image->height)/2;x=startX;y=startY;}
                else{x=clip.x+alignX+startX;y=clip.y+alignY+startY;}
                x=(std::max)(0,x);y=(std::max)(0,y);
                const int w=(std::min)(image->width,90-x),h=(std::min)(image->height,155-y);
                for(int row=0;row<h;++row)for(int col=0;col<w;++col){
                    const int sx=(clip.flags&1)?w-1-col:col;
                    const auto source=static_cast<std::size_t>(row)*image->width+sx;
                    u32 pixel=0;if(source<image->rgba.size())pixel=image->rgba[source];
                    else if(source<image->indices.size()){const auto index=image->indices[source];if(index)pixel=spr->m_pal[index]|0xff000000u;}
                    const unsigned alpha=pixel>>24;if(!alpha)continue;
                    auto& dest=bitmap.pixels[(y+row)*90+x+col];
                    if(alpha==255){dest=pixel;continue;}
                    const unsigned oldAlpha=dest>>24,newAlpha=alpha+(oldAlpha*(255-alpha)+127)/255;
                    u32 result=newAlpha<<24;
                    for(int shift:{0,8,16}){
                        const unsigned value=(((pixel>>shift)&255)*alpha+(((dest>>shift)&255)*oldAlpha*(255-alpha)+127)/255+newAlpha/2)/newAlpha;
                        result|=(value&255)<<shift;
                    }
                    dest=result;
                }
            }
        }
    }
    m_portrait.uri=ChatRoomBitmapUri(m_portrait.bitmap);
    // Bitmap/PNG projection uses straight ARGB; the native compositor takes
    // premultiplied channels, including the SPR's semitransparent RGBA clips.
    m_nativePortraitPixels=m_portrait.bitmap.pixels;
    for(auto& pixel:m_nativePortraitPixels){const u32 alpha=pixel>>24;u32 value=alpha<<24;
        for(int shift:{0,8,16})value|=((((pixel>>shift)&255)*alpha+127)/255)<<shift;pixel=value;}
    return m_portrait;
}

void UIPetInfoWnd::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;const int saved=SaveDC(dc);IntersectClipRect(dc,m_x,m_y,m_x+m_w,m_y+m_h);
    shopui::FillRectColor(dc,{m_x,m_y,m_x+m_w,m_y+m_h},RGB(255,255,255));
    auto pieces=GetFramePieces();const auto paint=[&](int phase){for(const auto& piece:pieces)if(piece.phase==phase)shopui::DrawBitmapPixelsTransparent(dc,piece.bitmap,{m_x+piece.x,m_y+piece.y,m_x+piece.x+piece.bitmap.width,m_y+piece.y+piece.bitmap.height});};paint(0);
    shopui::FillRectColor(dc,{m_x,m_y+17,m_x+1,m_y+m_h-21},RGB(128,128,128));
    shopui::FillRectColor(dc,{m_x+m_w-1,m_y+17,m_x+m_w,m_y+m_h-21},RGB(128,128,128));
    paint(1);
    if(g_session.m_petStatus.job>=1000 && g_session.m_petStatus.job<1986)shopui::FillRectColor(dc,{m_x+160,m_y+44,m_x+270,m_y+62},RGB(230,230,250));
    for(const auto& text:GetDisplayTexts())Text(dc,m_x+text.x,m_y+text.y,m_w-text.x,text.text,text.color);
    const auto& portrait=GetPortrait();if(portrait.bitmap.IsValid())AlphaBlendArgbToHdc(dc,m_x+portrait.x,m_y+portrait.y,portrait.bitmap.width,portrait.bitmap.height,m_nativePortraitPixels.data(),portrait.bitmap.width,portrait.bitmap.height);
    DrawChildrenToHdc(dc);RestoreDC(dc,saved);ReleaseDrawTarget(dc);
}
void UIPetInfoWnd::ShowMessage(int id,const std::string& text,std::function<void(bool)> action)
{
    if(g_windowMgr.m_modalWindow)return;
    CancelPointerCapture();if(g_hMainWnd && GetCapture()==g_hMainWnd)ReleaseCapture();
    auto* box=new UIChatRoomMessageBox(Utf8(text),id);box->SetSourceOwner(this);
    if(action)box->ConfigureConfirmation(this,std::move(action));
    g_windowMgr.m_roomMessageBox=box;g_windowMgr.m_children.push_back(box);g_windowMgr.m_modalWindow=box;g_windowMgr.m_editWindow=nullptr;
}
void UIPetInfoWnd::RequestRename()
{
    const std::string name=m_nameEdit->GetText();
    if(name.size()>=24){ShowMessage(682,Label(682));return;}
    ShowMessage(683,FormatName(Label(683),name),[this](bool accepted){
        if(!accepted || !g_windowMgr.ContainsWindow(this))return;
        const std::string name=m_nameEdit->GetText(); // Source rereads after its modal returns.
        g_modeMgr.SendMsg(158,reinterpret_cast<msgparam_t>(name.c_str()),0,0);
        g_session.m_petStatus.modified=1;g_session.m_petStatus.name=name;
        m_buttons[1]->Move(m_x-100,m_y-100);m_nameEdit->Move(m_x-100,m_y-100);Invalidate();
    });
}
void UIPetInfoWnd::ShowCommandMenu()
{
    CloseCommandMenu();m_menu=new UIPetCommandMenu(this);m_menu->Move(m_x+160,m_y+44);
    g_windowMgr.m_children.push_back(m_menu);g_windowMgr.m_editWindow=m_menu;
}
void UIPetInfoWnd::CloseCommandMenu(){if(m_menu)g_windowMgr.DeleteWindow(m_menu);}
void UIPetInfoWnd::CommandMenuDeleted(UIPetCommandMenu* menu){if(m_menu==menu)m_menu=nullptr;}
void UIPetInfoWnd::ConfirmFeed(UIWindow* owner)
{
    if(g_windowMgr.m_modalWindow)return;
    if(auto* pet=dynamic_cast<UIPetInfoWnd*>(owner))pet->CancelPointerCapture();
    if(g_hMainWnd && GetCapture()==g_hMainWnd)ReleaseCapture();
    const auto generation=g_modeMgr.GetOperationGeneration();
    auto* box=new UIChatRoomMessageBox(Utf8(Label(601)),601);
    box->ConfigureConfirmation(owner,[generation](bool accepted){
        if(accepted && g_modeMgr.GetOperationGeneration()==generation)
            g_modeMgr.SendMsg(163,1,0,0);
    });
    g_windowMgr.m_roomMessageBox=box;g_windowMgr.m_children.push_back(box);
    g_windowMgr.m_modalWindow=box;g_windowMgr.m_editWindow=nullptr;
}
void UIPetInfoWnd::ReturnEggLocally()
{
    const auto& pet=g_session.m_petStatus;
    if(const auto* source=g_session.GetInventoryItemByIndex(static_cast<unsigned int>(pet.eggIndex))){auto item=*source;item.m_isDamaged=0;g_session.SetInventoryItem(item);}
    g_session.m_petStatus.eggIndex=-1;g_session.m_petStatus.job=-1;
    if(g_windowMgr.m_itemWnd)g_windowMgr.m_itemWnd->Invalidate();
    if(g_windowMgr.m_petInfoWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_petInfoWnd);
}
void UIPetInfoWnd::SelectCommand(int index)
{
    if(index<0 || index>=4)return;
    if(index==0){ConfirmFeed(this);return;}
    g_modeMgr.SendMsg(163,index==1?2:index==2?4:3,0,0);
    if(index==3)ReturnEggLocally();
}
msgresult_t UIPetInfoWnd::SendMsg(UIWindow*,int message,msgparam_t value,msgparam_t,msgparam_t)
{
    if(message==34){
        m_info=reinterpret_cast<SubChatWindowInfo*>(value);if(!m_info)return 0;
        const auto client=g_windowMgr.GetChatClientRect();
        if(m_info->width<=0 || m_info->height<=0 || m_info->x<0 || m_info->y<0 || m_info->x>client.right-m_info->width || m_info->y>client.bottom-m_info->height)*m_info={150,100,280,200};
        Move(m_info->x,m_info->y);
    }else if(message==39)SelectCommand(static_cast<int>(value));
    else if(message==6){if(value==135)g_windowMgr.DeleteWindow(this);else if(value==118)RequestRename();else if(value==202)ShowCommandMenu();}
    else if(message==0 || message==1)SendMsg(this,6,message==0?m_defPushId:m_defCancelPushId,0,0);
    return 0;
}

UIPetCommandMenu::UIPetCommandMenu(UIPetInfoWnd* owner):m_owner(owner)
{
    m_id=26;Create(110,64);const int ids[]={592,593,595,594};for(int i=0;i<4;++i)m_labels[i]=Utf8(Label(ids[i]));
}
UIPetCommandMenu::~UIPetCommandMenu(){m_owner->CommandMenuDeleted(this);if(g_windowMgr.ContainsWindow(m_owner))g_windowMgr.m_editWindow=m_owner;}
void UIPetCommandMenu::Activate(int index){auto* owner=m_owner;g_windowMgr.DeleteWindow(this);if(g_windowMgr.ContainsWindow(owner))owner->SelectCommand(index);}
void UIPetCommandMenu::OnLBtnUp(int x,int y){if(x>=m_x && x<m_x+m_w && y>=m_y && y<m_y+m_h)Activate((y-m_y)/16);}
void UIPetCommandMenu::OnMouseMove(int x,int y){const int index=x>=m_x && x<m_x+m_w && y>=m_y && y<m_y+m_h?(y-m_y)/16:-1;if(index!=m_selection){m_selection=index;Invalidate();}}
void UIPetCommandMenu::OnKeyDown(int key){if(key==VK_ESCAPE){Activate(-1);return;}if(key==VK_RETURN){Activate(m_selection);return;}if(key==VK_UP || key==VK_DOWN){m_selection=(std::max)(0,(std::min)(3,m_selection+(key==VK_UP?-1:1)));Invalidate();}}
void UIPetCommandMenu::OnDraw()
{
    auto dc=AcquireDrawTarget();if(!dc)return;const int saved=SaveDC(dc);IntersectClipRect(dc,m_x,m_y,m_x+m_w,m_y+m_h);shopui::FillRectColor(dc,{m_x,m_y,m_x+m_w,m_y+m_h},RGB(240,240,240));
    for(int i=0;i<4;++i){if(i==m_selection)shopui::FillRectColor(dc,{m_x,m_y+16*i,m_x+m_w,m_y+16*i+16},RGB(50,200,200));Text(dc,m_x+3,m_y+16*i,m_w-3,m_labels[i],0);}
    RestoreDC(dc,saved);ReleaseDrawTarget(dc);
}
