#include "ui/UIChatRoomSettingsWnd.h"
#include "ui/UIPlayerContextMenuWnd.h"
#include "core/MannerFilter.h"

void CheckChatRoomSettingsUi(QGuiApplication& app,QQuickView& view,QtUiState* state,
    CGameMode& mode,QtUiStateAdapter& adapter,const QString& output)
{
    g_windowMgr.RemoveAllWindows();
    struct Restore {
        int scale=GetConfiguredUiScalePercent(),folder=g_readFolderFirst;
        std::filesystem::path cwd=std::filesystem::current_path();
        ro::MannerFilter filter=ro::GetMannerFilter();ro::commerce::State commerce=ro::commerce::state;
        ~Restore(){g_windowMgr.RemoveAllWindows();SetRuntimeUiScalePercent(scale);g_readFolderFirst=folder;
            std::filesystem::current_path(cwd);ro::GetMannerFilter()=std::move(filter);ro::commerce::state=std::move(commerce);}
    } restore;
    const auto assets=std::filesystem::path(output.toStdString())/"entry-assets";
    std::filesystem::current_path(assets);g_readFolderFirst=1;SetRuntimeUiScalePercent(100);ro::commerce::state={};
    ro::GetMannerFilter().Clear();ro::GetMannerFilter().AppendEncodedFile("BAD\r\n",65001);
    const std::string invitation = " wishes to adopt you. Do you accept?";
    const std::string warning = "When you become a child, you will be unable to become a Transcendent Class character, all stats will be limited to a maximum of 80, and Max HP/SP will be reduced. Are you sure that you want to be adopted?";
    {
        std::ofstream f("msgstringtable.txt", std::ios::binary);
        for (int i = 0; i <= 888; ++i) {
            f << (i == 14 ? "" : i == 794 ? invitation : i == 888 ? warning : "Settings label") << "#\r\n";
        }
    }
    const auto pointer=[&](UINT message,int x,int y){WindowProc(g_hMainWnd,message,message==WM_LBUTTONUP?0:MK_LBUTTON,
        MAKELPARAM(UiScaleLogicalToRawCoordinate(x),UiScaleLogicalToRawCoordinate(y)));};
    const auto tap=[&](int x,int y){pointer(WM_LBUTTONDOWN,x,y);pointer(WM_LBUTTONUP,x,y);};
    const auto capture=[&](const QString& name){
        state->setUiScale(GetConfiguredUiScaleFactor());adapter.syncChat();Frames(app,view);
        auto* form=g_windowMgr.m_chatRoomMakeWnd?g_windowMgr.m_chatRoomMakeWnd:g_windowMgr.m_chatRoomChangeWnd;
        Require(form,"settings render lost managed owner");
        auto* item=Find(view.rootObject(),form->IsChange()?"sourceRoomChange":"sourceRoomMake");
        Require(item && item->isVisible(),"actual QML settings owner missing");
        const auto pos=item->mapToScene(QPointF(0,0));
        Require(qRound(pos.x())==UiScaleLogicalToRawCoordinate(form->m_x) && qRound(pos.y())==UiScaleLogicalToRawCoordinate(form->m_y),"settings QML source placement/scale diverged");
        const auto settings=state->chatWindowUi().value("roomSettings").toList();
        Require(settings.size()==1 && !settings[0].toMap().contains("password")
            && settings[0].toMap().value("masked").toString()==QString(static_cast<int>(form->GetPasswordText().size()),QChar('*')),"settings projection exposed password or lost mask");
        ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"settings native DIB failed");
        std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);form->DrawToHdc(dib.GetDC());
        if(g_windowMgr.m_roomMessageBox)g_windowMgr.m_roomMessageBox->DrawToHdc(dib.GetDC());
        auto native=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy();
        if(GetConfiguredUiScalePercent()!=100)native=native.scaled(qRound(native.width()*GetConfiguredUiScaleFactor()),qRound(native.height()*GetConfiguredUiScaleFactor()),Qt::IgnoreAspectRatio,Qt::FastTransformation).copy(0,0,view.width(),view.height());
        const auto qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        if(!g_windowMgr.m_roomMessageBox){const int x=UiScaleLogicalToRawCoordinate(form->m_x+194),y=UiScaleLogicalToRawCoordinate(form->m_y+101);
            const QColor expected=form->GetPressedButton()==118?QColor("#806040"):QColor("#c0a080");
            Require(native.pixelColor(x,y)==expected && qt.pixelColor(x,y)==expected,"settings actual native/QML OK artwork differs");}
        if(auto* message=g_windowMgr.m_roomMessageBox){auto* box=Find(view.rootObject(),"sourceRoomValidation");
            Require(box && box->isVisible() && box->z()>3000,"validation did not project above commerce dialogs");
            const auto point=box->mapToScene(QPointF(0,0));Require(qRound(point.x())==UiScaleLogicalToRawCoordinate(message->m_x)
                && qRound(point.y())==UiScaleLogicalToRawCoordinate(message->m_y),"modal source placement/scale differs");
            const int x=UiScaleLogicalToRawCoordinate(message->m_x+194),y=UiScaleLogicalToRawCoordinate(message->m_y+101);
            Require(qt.pixelColor(x,y)==QColor("#c0a080") && native.pixelColor(x,y)==QColor("#c0a080"),"validation button pixels missing above settings");}
        Require(native.save(output+"/"+name+".native.png") && qt.save(output+"/"+name+".qml.png"),"save settings frame failed");
    };
    Require(g_windowMgr.OnQtKeyDown('C',true,false,false),"actual Alt+C was not consumed");auto* make=g_windowMgr.m_chatRoomMakeWnd;
    Require(make,"Alt+C did not create settings owner");
    for(char c:std::string("Literal <title>"))WindowProc(g_hMainWnd,WM_CHAR,c,0);
    pointer(WM_LBUTTONDOWN,make->m_x+90,make->m_y+75);
    Require(make->IsPrivate(),"source radio did not dispatch on mouse down");pointer(WM_LBUTTONUP,-20,-20);
    tap(make->m_x+190,make->m_y+75);for(char c:std::string("secret123"))WindowProc(g_hMainWnd,WM_CHAR,c,0);
    Require(make->GetPasswordText()=="secret12","settings max8/native focused input failed");
    tap(make->m_x+122,make->m_y+50);Require(make->GetPopupKind()==0,"people combo did not open on release");
    make->OnWheel(-120);make->OnWheel(-120);make->OnWheel(-120);capture("chat-settings-combo");
    tap(make->m_x+50,make->m_y+100);Require(make->GetPeople()==20 && make->GetPopupKind()<0,"people popup final choice failed");
    pointer(WM_LBUTTONDOWN,make->m_x+194,make->m_y+101);Require(GetCapture()==g_hMainWnd,"settings button lacks actual capture");capture("chat-settings-pressed");
    pointer(WM_LBUTTONUP,-20,-20);Require(g_windowMgr.m_chatRoomMakeWnd==make && !GetCapture(),"outside release submitted settings or retained capture");
    const int x=make->m_x,y=make->m_y;pointer(WM_LBUTTONDOWN,x+3,y+115);pointer(WM_MOUSEMOVE,x+23,y+125);pointer(WM_LBUTTONUP,-20,-20);
    Require(make->m_x==x+20 && make->m_y==y+10 && !GetCapture(),"settings frame drag/outside release failed");
    pointer(WM_LBUTTONDOWN,make->m_x+3,make->m_y+115);WindowProc(g_hMainWnd,WM_CANCELMODE,0,0);
    Require(!g_windowMgr.m_captureWindow && !GetCapture(),"settings cancellation retained capture");
    make->SetTitleText("BAD");WindowProc(g_hMainWnd,WM_KEYDOWN,VK_RETURN,0);
    Require(g_windowMgr.m_roomMessageBox && g_windowMgr.m_roomMessageBox->GetText().empty(),"known-empty modal label missing");capture("chat-settings-validation-empty");
    WindowProc(g_hMainWnd,WM_CHAR,'z',0);Require(make->GetTitleText()=="BAD","modal leaked text into settings");
    WindowProc(g_hMainWnd,WM_KEYDOWN,VK_ESCAPE,0);Require(g_windowMgr.m_editWindow==make,"modal did not restore settings focus");
    g_windowMgr.DeleteWindow(make);g_windowMgr.MakeWindow(UIWindowMgr::WID_CHATROOMWND);
    mode.m_lastChatroomInfo={"Prior","abcd",0,3,2,0x11223344};g_windowMgr.m_chatRoomWnd->AddMember(g_session.GetPlayerName(),0);g_windowMgr.m_chatRoomWnd->OpenMemberMenu(0,0);g_windowMgr.m_chatRoomWnd->SendMsg(nullptr,48,1,0,0);
    auto* change=g_windowMgr.m_chatRoomChangeWnd;Require(change && change->GetPeople()==2 && change->GetPeopleLabel().find("20")==0,"actual settings prefill quirks changed");
    WindowProc(g_hMainWnd,WM_CHAR,'N',0);Require(change->GetTitleText()=="N","native selected title was not replaced");
    SetRuntimeUiScalePercent(125);capture("chat-settings-change-125");
    change->SetTitleText("");WindowProc(g_hMainWnd,WM_KEYDOWN,VK_RETURN,0);capture("chat-settings-validation-125");
    g_windowMgr.DeleteWindow(change);WindowProc(g_hMainWnd,WM_KEYDOWN,VK_RETURN,0);
    Require(!g_windowMgr.m_editWindow && !g_windowMgr.m_modalWindow,"modal restored destroyed settings editor");
    g_windowMgr.RemoveAllWindows();adapter.syncChat();Frames(app,view);
    Require(state->chatWindowUi().value("roomSettings").toList().empty() && !Find(view.rootObject(),"sourceRoomValidation")->isVisible(),"settings QML teardown retained owners");
    QFile result(output+"/chat-settings-results.json");Require(result.open(QIODevice::WriteOnly),"settings result open failed");
    result.write(QJsonDocument(QJsonObject{{"native_qml_frames",10},{"radio_down",true},{"combo",true},{"masked_native_input",true},
        {"capture_cancel_release",true},{"known_empty_modal",true},{"prefill",true},{"scale_125",true},{"deleted_editor",true},{"outcome","PASS"}}).toJson());

    SetRuntimeUiScalePercent(100);
    QImage cancelBitmap(40, 20, QImage::Format_RGB32);
    cancelBitmap.fill(QColor("#a0c080"));
    Require(cancelBitmap.save(QString::fromStdString((assets / "btn_cancel.bmp").string())),
        "adoption cancel fixture missing");
    const auto captureConsent = [&](int id, const std::string& expected, const QString& name) {
        auto* dialog = g_windowMgr.m_roomMessageBox;
        Require(dialog && dialog == g_windowMgr.m_modalWindow && dialog->HasCancel()
            && dialog->GetMessageId() == id && dialog->GetText() == expected,
            "adoption prompt did not retain its full localized text");
        state->setUiScale(GetConfiguredUiScaleFactor());
        adapter.syncChat();
        Frames(app, view);
        auto* box = Find(view.rootObject(), "sourceRoomValidation");
        auto* label = Find(view.rootObject(), "sourceRoomValidationText");
        Require(box && box->isVisible() && label && label->isVisible()
            && label->property("text").toString() == QString::fromStdString(expected)
            && label->property("contentHeight").toReal() <= label->height(),
            "Qt adoption prompt lost or clipped its text");
        const auto& ok = dialog->GetButton();
        const auto& cancel = dialog->GetCancelButton();
        Require(22 + dialog->GetMessageBodyHeight() <= std::min(ok.y, cancel.y),
            "adoption buttons overlap the message body");
        ArgbDibSurface dib;
        Require(dib.EnsureSize(view.width(), view.height()), "adoption native DIB failed");
        std::fill_n(dib.GetPixels(), view.width() * view.height(), 0xff181b20u);
        QFont font("Segoe UI");
        font.setPixelSize(12);
        Require(QFontMetrics(font).boundingRect(QRect(0, 0, 260, 1000), Qt::TextWordWrap,
            QString::fromStdString(expected)).height() <= dialog->GetMessageBodyHeight(),
            "native adoption prompt clips its text");
        dialog->DrawToHdc(dib.GetDC());
        const auto native = QImage(reinterpret_cast<uchar*>(dib.GetPixels()), view.width(), view.height(),
            view.width() * 4, QImage::Format_RGB32).copy();
        const auto qt = view.grabWindow().convertToFormat(QImage::Format_RGB32);
        for (const auto& image : {native, qt}) {
            Require(image.pixelColor(dialog->m_x + ok.x + 5, dialog->m_y + ok.y + 5) == QColor("#c0a080")
                && image.pixelColor(dialog->m_x + cancel.x + 5, dialog->m_y + cancel.y + 5) == QColor("#a0c080"),
                "adoption confirmation buttons did not render");
        }
        Require(native.save(output + "/" + name + ".native.png")
            && qt.save(output + "/" + name + ".qml.png"), "could not save adoption frames");
    };
    mode.ReceiveAdoptionInvitation(100, 200, "Parent");
    captureConsent(794, "Parent" + invitation, "adoption-invitation");
    auto* dialog = g_windowMgr.m_roomMessageBox;
    tap(dialog->m_x + dialog->GetButton().x + 5, dialog->m_y + dialog->GetButton().y + 5);
    captureConsent(888, warning, "adoption-consequences");
    mode.ClearAdoptionInvitation();
    mode.UpdateAdoptionInvitation();
    adapter.syncChat();
    Frames(app, view);
    Require(!g_windowMgr.m_roomMessageBox && !g_windowMgr.m_modalWindow
        && !Find(view.rootObject(), "sourceRoomValidation")->isVisible(),
        "adoption teardown retained the warning");
}

void CheckAdoptionParentMenu(QGuiApplication& app, QQuickView& view, QtUiState* state,
    CGameMode& mode, const QString& output)
{
    auto* world = mode.m_world;
    Require(world, "adoption parent menu fixture needs a world");
    CGameActor candidate;
    candidate.m_gid = 0xabc001;
    candidate.m_isPc = 1;
    candidate.m_job = 0;
    world->RegisterActor(&candidate);
    CHARACTER_INFO parent{};
    parent.level = 70;
    parent.job = 7;
    g_session.SetSelectedCharacterAppearance(parent);
    PLAYER_SKILL_INFO recall{};
    recall.SKID = 336;
    recall.m_isValid = 1;
    recall.level = 1;
    g_session.SetSkillItem(recall);
    FRIEND_INFO member{};
    member.isValid = 1;
    member.AID = candidate.m_gid;
    member.characterName = "Candidate";
    g_session.m_partyList = {member};
    SetRuntimeUiScalePercent(100);
    state->setUiScale(1.0);
    const auto capture = [&](bool eligible, const QString& name) {
        Require(mode.OpenPlayerContextMenu(candidate.m_gid, "Candidate", 60, 60),
            "could not open the production player menu");
        auto* menu = g_windowMgr.m_playerContextMenuWnd;
        const auto& entries = menu->GetEntries();
        const auto count = std::count_if(entries.begin(), entries.end(), [](const auto& entry) {
            return entry.actionId == CGameMode::PlayerContext_RequestAdoption;
        });
        Require(count == (eligible ? 1 : 0), "native adoption menu eligibility differs");
        PopulateCommerceUi(state, mode, false);
        Frames(app, view);
        auto* panel = Find(view.rootObject(), "commerce_playerMenu");
        auto* row = Find(view.rootObject(), "commerceContext10");
        Require(panel && panel->isVisible() && (row && row->isVisible()) == eligible,
            "Qt adoption menu eligibility differs");

        ArgbDibSurface dib;
        Require(dib.EnsureSize(view.width(), view.height()), "adoption menu native DIB failed");
        std::fill_n(dib.GetPixels(), view.width() * view.height(), 0xff181b20u);
        menu->DrawMenu(dib.GetDC());
        const auto native = QImage(reinterpret_cast<uchar*>(dib.GetPixels()), view.width(), view.height(),
            view.width() * 4, QImage::Format_RGB32).copy();
        const auto qt = view.grabWindow().convertToFormat(QImage::Format_RGB32);
        if (eligible) {
            Require(entries.back().label.find("Candidate") != std::string::npos
                && row->property("label").toString() == QString::fromStdString(entries.back().label),
                "adoption action label lost the candidate name");
            const auto qtBounds = row->mapRectToScene(QRectF(0, 0, row->width(), row->height())).toAlignedRect();
            const auto panelBounds = panel->mapRectToScene(QRectF(0, 0, panel->width(), panel->height())).toAlignedRect();
            Require(qt.rect().contains(qtBounds) && panelBounds.contains(qtBounds),
                "Qt adoption row extends outside its visible menu");
            // The final native row is surrounded by 24 pixels of bottom padding.
            const QRect nativeRow(menu->m_x + 12, menu->m_y + menu->m_h - 54, 220, 30);
            int textPixels = 0;
            for (int y = nativeRow.top(); y <= nativeRow.bottom(); ++y) {
                for (int x = nativeRow.left(); x <= nativeRow.right(); ++x) {
                    if (native.pixelColor(x, y).value() < 80) ++textPixels;
                }
            }
            Require(textPixels > 40, "native adoption row has no rendered text");
        }
        Require(native.save(output + "/" + name + ".native.png")
            && qt.save(output + "/" + name + ".qml.png"), "could not save adoption parent menu frames");
        g_windowMgr.ClosePlayerContextMenu();
    };
    capture(true, "adoption-parent-eligible");
    candidate.m_job = 4023;
    capture(false, "adoption-parent-ineligible");
    world->UnregisterActor(&candidate);
    PopulateCommerceUi(state, mode, false);
}
