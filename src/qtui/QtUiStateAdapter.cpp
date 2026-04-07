#include "QtUiStateAdapter.h"

#include "QtUiState.h"

#include "network/Connection.h"
#include "network/Packet.h"
#include "core/ClientInfoLocale.h"
#include "gamemode/GameMode.h"
#include "gamemode/View.h"
#include "main/WinMain.h"
#include "render3d/RenderBackend.h"
#include "render3d/RenderDevice.h"
#include "session/Session.h"
#include "ui/UILoginWnd.h"
#include "ui/UILoadingWnd.h"
#include "ui/UIMakeCharWnd.h"
#include "ui/UIChooseWnd.h"
#include "ui/UIChooseSellBuyWnd.h"
#include "ui/UIBasicInfoWnd.h"
#include "ui/UIEquipWnd.h"
#include "ui/UIItemWnd.h"
#include "ui/UIItemPurchaseWnd.h"
#include "ui/UIItemSellWnd.h"
#include "ui/UIItemShopWnd.h"
#include "ui/UIMinimapWnd.h"
#include "ui/UINewChatWnd.h"
#include "ui/UINpcMenuWnd.h"
#include "ui/UINpcInputWnd.h"
#include "ui/UINotifyLevelUpWnd.h"
#include "ui/UIOptionWnd.h"
#include "ui/UIRechargeGage.h"
#include "ui/UISelectCharWnd.h"
#include "ui/UISelectServerWnd.h"
#include "ui/UISayDialogWnd.h"
#include "ui/UISkillListWnd.h"
#include "ui/UIShortCutWnd.h"
#include "ui/UIStatusWnd.h"
#include "ui/UIShopCommon.h"
#include "ui/UIWaitWnd.h"
#include "ui/UIWindowMgr.h"
#include "world/GameActor.h"
#include "world/World.h"

#include <QChar>
#include <QFont>
#include <QFontMetrics>
#include <QStringList>
#include <QVariantMap>

#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr DWORD kHoverNameRequestCooldownMs = 1000;
constexpr int kQtActorLabelVerticalOffset = 10;

QString ToQString(const std::string& value)
{
    return QString::fromStdString(value);
}

QString ToQString(const char* value)
{
    return value ? QString::fromLocal8Bit(value) : QString();
}

QString FormatCompositeStatusText(int primary, int secondary)
{
    if (secondary > 0) {
        return QStringLiteral("%1 + %2").arg(primary).arg(secondary);
    }
    return QString::number(primary);
}

QString FormatMatkStatusText(int minValue, int maxValue)
{
    if (minValue == maxValue) {
        return QString::number(maxValue);
    }
    return QStringLiteral("%1~%2").arg(minValue).arg(maxValue);
}

QString FormatBaseStatusText(int baseValue, int plusValue)
{
    if (plusValue > 0) {
        return QStringLiteral("%1+%2").arg(baseValue).arg(plusValue);
    }
    return QString::number(baseValue);
}

QString FormatBasicGaugeText(const char* label, int current, int maximum)
{
    return QStringLiteral("%1      %2  /  %3")
        .arg(ToQString(label))
        .arg(current)
        .arg(maximum);
}

QString FormatBasicLevelText(const char* label, int value)
{
    return QStringLiteral("%1 %2")
        .arg(ToQString(label))
        .arg(value);
}

QString FormatBasicInfoMiniHeaderText(int level, const QString& jobName, int expPercent)
{
    return QStringLiteral("Lv. %1 / %2 / Exp. %3 %")
        .arg(level)
        .arg(jobName)
        .arg(expPercent);
}

QString FormatBasicInfoMiniStatusText(int hp, int maxHp, int sp, int maxSp, int money)
{
    return QStringLiteral("HP %1 / %2  |  SP %3 / %4  |  %5 Z")
        .arg(hp)
        .arg(maxHp)
        .arg(sp)
        .arg(maxSp)
        .arg(money);
}

QString FormatCharacterSlotLevelText(int level)
{
    return QStringLiteral("Lv. %1").arg(level);
}

QVariantMap BuildDebugOverlayData(const QString& backendName,
    const QString& modeName,
    const QString& renderPath,
    const QString& loginStatus,
    const QString& chatPreview,
    const QString& lastInput)
{
    QVariantMap data;
    data.insert(QStringLiteral("title"), QStringLiteral("Qt 6 GPU UI"));
    data.insert(QStringLiteral("backendLine"), QStringLiteral("Backend: %1").arg(backendName));
    data.insert(QStringLiteral("modeLine"), QStringLiteral("Mode: %1").arg(modeName));
    data.insert(QStringLiteral("renderPathLine"), QStringLiteral("Render path: %1").arg(renderPath));
    data.insert(QStringLiteral("loginStatusLine"), QStringLiteral("Login status: %1").arg(loginStatus));
    data.insert(QStringLiteral("chatPreviewText"), QStringLiteral("Recent chat:\n%1").arg(chatPreview));
    data.insert(QStringLiteral("inputLine"), QStringLiteral("Input: %1").arg(lastInput));
    return data;
}

bool IsNpcColorCodeAt(const std::string& value, size_t index)
{
    if (index + 7 > value.size() || value[index] != '^') {
        return false;
    }
    for (size_t offset = 1; offset <= 6; ++offset) {
        const char ch = value[index + offset];
        const bool hex = (ch >= '0' && ch <= '9')
            || (ch >= 'A' && ch <= 'F')
            || (ch >= 'a' && ch <= 'f');
        if (!hex) {
            return false;
        }
    }
    return true;
}

QString NpcColorCodesToHtml(const std::string& value)
{
    QString html = QStringLiteral("<span style=\"color:#000000;\">");
    size_t segmentStart = 0;
    for (size_t index = 0; index < value.size();) {
        if (IsNpcColorCodeAt(value, index)) {
            if (index > segmentStart) {
                html += ToQString(value.substr(segmentStart, index - segmentStart)).toHtmlEscaped();
            }
            html += QStringLiteral("</span><span style=\"color:#%1;\">")
                .arg(QString::fromLatin1(value.data() + index + 1, 6).toLower());
            index += 7;
            segmentStart = index;
            continue;
        }
        if (value[index] == '\r') {
            if (index > segmentStart) {
                html += ToQString(value.substr(segmentStart, index - segmentStart)).toHtmlEscaped();
            }
            if (index + 1 < value.size() && value[index + 1] == '\n') {
                ++index;
            }
            html += QStringLiteral("<br/>");
            ++index;
            segmentStart = index;
            continue;
        }
        if (value[index] == '\n') {
            if (index > segmentStart) {
                html += ToQString(value.substr(segmentStart, index - segmentStart)).toHtmlEscaped();
            }
            html += QStringLiteral("<br/>");
            ++index;
            segmentStart = index;
            continue;
        }
        ++index;
    }
    if (segmentStart < value.size()) {
        html += ToQString(value.substr(segmentStart)).toHtmlEscaped();
    }
    html += QStringLiteral("</span>");
    return html;
}

QString ResolveShortcutSlotLabel(const SHORTCUT_SLOT* slot)
{
    if (!slot || slot->id == 0) {
        return QString();
    }

    if (slot->isSkill != 0) {
        if (const PLAYER_SKILL_INFO* skill = g_session.GetSkillItemBySkillId(static_cast<int>(slot->id))) {
            if (!skill->skillName.empty()) {
                return ToQString(skill->skillName);
            }
            if (!skill->skillIdName.empty()) {
                return ToQString(skill->skillIdName);
            }
        }
        return QStringLiteral("Skill %1").arg(slot->id);
    }

    if (const ITEM_INFO* item = g_session.GetInventoryItemByItemId(slot->id)) {
        const std::string displayName = shopui::GetItemDisplayName(*item);
        if (!displayName.empty()) {
            return ToQString(displayName);
        }
    }

    return QStringLiteral("Item %1").arg(slot->id);
}

QString ToCssColor(u8 red, u8 green, u8 blue)
{
    return QStringLiteral("#%1%2%3")
        .arg(static_cast<int>(red), 2, 16, QChar('0'))
        .arg(static_cast<int>(green), 2, 16, QChar('0'))
        .arg(static_cast<int>(blue), 2, 16, QChar('0'));
}

QString BackendToQString(RenderBackendType backend)
{
    return QString::fromLatin1(GetRenderBackendName(backend));
}

QString BuildRenderPathText(RenderBackendType nativeOverlayBackend)
{
    if (nativeOverlayBackend != RenderBackendType::LegacyDirect3D7) {
        return QStringLiteral("Native GPU Qt overlay on %1").arg(BackendToQString(nativeOverlayBackend));
    }
    return QStringLiteral("CPU bridge fallback");
}

QString BuildArchitectureNote(RenderBackendType nativeOverlayBackend)
{
    QtUiRenderTargetInfo targetInfo{};
    const bool nativeTargetAvailable = GetRenderDevice().GetQtUiRenderTargetInfo(&targetInfo);
    const QString backendName = BackendToQString(targetInfo.backend);

    if (nativeTargetAvailable && targetInfo.available) {
        if (nativeOverlayBackend != RenderBackendType::LegacyDirect3D7) {
            return QStringLiteral("Qt Quick is rendering into a native %1 overlay texture and the renderer composites that texture in the GPU overlay pass.")
                .arg(BackendToQString(nativeOverlayBackend));
        }
        if (targetInfo.backend == RenderBackendType::Vulkan) {
            return QStringLiteral("Renderer-native Vulkan target is available for Qt UI (%1x%2). Legacy paths can now be retired feature-by-feature.")
                .arg(targetInfo.width)
                .arg(targetInfo.height);
        }

        return QStringLiteral("Renderer-owned Qt target descriptor is available on %1 (%2x%3). Remaining legacy paths should move onto the native GPU overlay.")
            .arg(backendName)
            .arg(targetInfo.width)
            .arg(targetInfo.height);
    }

    return QStringLiteral("Qt runtime is active, but %1 still falls back to the CPU bridge until a native target is available.")
        .arg(backendName);
}

QString BuildChatPreviewText()
{
    const std::vector<UIChatEvent> preview = g_windowMgr.GetChatPreviewEvents(3);
    if (preview.empty()) {
        return QStringLiteral("No recent chat events.");
    }

    QStringList lines;
    for (const UIChatEvent& event : preview) {
        lines.push_back(ToQString(event.text));
    }
    return lines.join(QLatin1Char('\n'));
}

QString BuildMenuModeText()
{
    if (g_windowMgr.m_loadingWnd && g_windowMgr.m_loadingWnd->m_show != 0) {
        return QStringLiteral("Loading");
    }
    if (g_windowMgr.m_waitWnd && g_windowMgr.m_waitWnd->m_show != 0) {
        return QStringLiteral("Please Wait");
    }
    if (g_windowMgr.m_makeCharWnd && g_windowMgr.m_makeCharWnd->m_show != 0) {
        return QStringLiteral("Character Create");
    }
    if (g_windowMgr.m_selectCharWnd && g_windowMgr.m_selectCharWnd->m_show != 0) {
        return QStringLiteral("Character Select");
    }
    if (g_windowMgr.m_selectServerWnd && g_windowMgr.m_selectServerWnd->m_show != 0) {
        return QStringLiteral("Server Select");
    }
    if (g_windowMgr.m_loginWnd && g_windowMgr.m_loginWnd->m_show != 0) {
        return QStringLiteral("Login");
    }
    return QStringLiteral("Menu");
}

void PopulateLoadingState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UILoadingWnd* const loadingWnd = g_windowMgr.m_loadingWnd;
    const UIWaitWnd* const waitWnd = g_windowMgr.m_waitWnd;
    const bool loadingVisible = (loadingWnd && loadingWnd->m_show != 0)
        || (waitWnd && waitWnd->m_show != 0);
    state->setLoadingVisible(loadingVisible);
    if (loadingWnd && loadingWnd->m_show != 0) {
        state->setLoadingMessage(ToQString(loadingWnd->GetMessage().c_str()));
        state->setLoadingProgress(static_cast<double>(loadingWnd->GetProgress()));
    } else if (waitWnd && waitWnd->m_show != 0) {
        state->setLoadingMessage(ToQString(waitWnd->m_waitMsg.c_str()));
        state->setLoadingProgress(0.0);
    } else {
        state->setLoadingMessage(QString());
        state->setLoadingProgress(0.0);
    }
}

void PopulateServerSelectState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UISelectServerWnd* const serverWnd = g_windowMgr.m_selectServerWnd;
    const bool visible = serverWnd && serverWnd->m_show != 0 && GetClientInfoConnectionCount() > 1;
    state->setServerSelectVisible(visible);
    if (!visible) {
        state->setServerPanelGeometry(0, 0, 0, 0);
        state->setServerSelectedIndex(-1);
        state->setServerHoverIndex(-1);
        state->setServerPanelData(QVariantMap{});
        state->setServerEntries(QVariantList{});
        state->setServerEntryText(QStringList{}, QStringList{});
        return;
    }

    if (g_windowMgr.m_loginWnd && g_windowMgr.m_loginWnd->m_show != 0) {
        g_windowMgr.m_loginWnd->EnsureQtLayout();
    }
    g_windowMgr.m_selectServerWnd->SetShow(1);

    state->setServerPanelGeometry(serverWnd->m_x, serverWnd->m_y, serverWnd->m_w, serverWnd->m_h);
    state->setServerSelectedIndex(GetSelectedClientInfoIndex());
    state->setServerHoverIndex(serverWnd->GetHoverIndex());
    QVariantMap serverPanelData;
    serverPanelData.insert(QStringLiteral("title"), QStringLiteral("Select Server"));
    state->setServerPanelData(serverPanelData);

    QVariantList entries;
    QStringList labels;
    QStringList details;
    const std::vector<ClientInfoConnection>& connections = GetClientInfoConnections();
    entries.reserve(static_cast<qsizetype>(connections.size()));
    labels.reserve(static_cast<qsizetype>(connections.size()));
    details.reserve(static_cast<qsizetype>(connections.size()));
    for (const ClientInfoConnection& info : connections) {
        const QString label = ToQString((!info.display.empty() ? info.display : info.address).c_str());
        const QString detail = ToQString((!info.desc.empty() ? info.desc : info.port).c_str());
        QVariantMap entry;
        entry.insert(QStringLiteral("label"), label);
        entry.insert(QStringLiteral("detail"), detail);
        entries.push_back(entry);
        labels.push_back(label);
        details.push_back(detail);
    }
    state->setServerEntries(entries);
    state->setServerEntryText(labels, details);
}

void PopulateLoginPanelState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UILoginWnd* const loginWnd = g_windowMgr.m_loginWnd;
    const bool visible = loginWnd && loginWnd->m_show != 0;
    state->setLoginPanelVisible(visible);
    if (!visible) {
        state->setLoginPanelGeometry(0, 0, 0, 0);
        state->setLoginPanelData(QString(), QString(), false, false);
        state->setLoginPanelLabels(QVariantMap{});
        state->setLoginButtons(QVariantList{});
        return;
    }

    g_windowMgr.m_loginWnd->EnsureQtLayout();
    g_windowMgr.m_loginWnd->RefreshRememberedUserIdStorage();
    state->setLoginPanelGeometry(loginWnd->m_x, loginWnd->m_y, loginWnd->m_w, loginWnd->m_h);
    const QString userId = ToQString(loginWnd->GetLoginText());
    const QString passwordMask(loginWnd->GetPasswordLength(), QLatin1Char('*'));
    state->setLoginPanelData(
        userId,
        passwordMask,
        loginWnd->IsSaveAccountChecked(),
        loginWnd->IsPasswordFocused());
    QVariantMap loginPanelLabels;
    loginPanelLabels.insert(QStringLiteral("title"), QStringLiteral("Login"));
    loginPanelLabels.insert(QStringLiteral("saveLabel"), QStringLiteral("Save"));
    state->setLoginPanelLabels(loginPanelLabels);

    QVariantList buttons;
    const int buttonCount = loginWnd->GetQtButtonCount();
    buttons.reserve(buttonCount);
    for (int index = 0; index < buttonCount; ++index) {
        UILoginWnd::QtButtonDisplay buttonDisplay{};
        if (!loginWnd->GetQtButtonDisplayForQt(index, &buttonDisplay)) {
            continue;
        }

        QVariantMap button;
        button.insert(QStringLiteral("id"), buttonDisplay.id);
        button.insert(QStringLiteral("x"), buttonDisplay.x);
        button.insert(QStringLiteral("y"), buttonDisplay.y);
        button.insert(QStringLiteral("width"), buttonDisplay.width);
        button.insert(QStringLiteral("height"), buttonDisplay.height);
        button.insert(QStringLiteral("label"), ToQString(buttonDisplay.label));
        buttons.push_back(button);
    }
    state->setLoginButtons(buttons);
}

void PopulateCharSelectState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UISelectCharWnd* const selectCharWnd = g_windowMgr.m_selectCharWnd;
    const bool visible = selectCharWnd && selectCharWnd->m_show != 0;
    state->setCharSelectVisible(visible);
    if (!visible) {
        state->setCharSelectPanelGeometry(0, 0, 0, 0);
        state->setCharSelectPageState(0, 0);
        state->setCharSelectSlots(QVariantList{});
        state->setCharSelectSelectedDetails(QVariantMap{});
        state->setCharSelectPageButtons(QVariantList{});
        state->setCharSelectActionButtons(QVariantList{});
        return;
    }

    g_windowMgr.m_selectCharWnd->EnsureQtLayout();
    state->setCharSelectPanelGeometry(
        selectCharWnd->m_x,
        selectCharWnd->m_y,
        selectCharWnd->m_w,
        selectCharWnd->m_h);
    state->setCharSelectPageState(
        selectCharWnd->GetCurrentPage(),
        selectCharWnd->GetCurrentPageCount());

    QVariantList slotList;
    QStringList imageRevisionParts;
    imageRevisionParts.reserve(4);
    imageRevisionParts.push_back(QStringLiteral("page=%1").arg(selectCharWnd->GetCurrentPage()));
    imageRevisionParts.push_back(QStringLiteral("pages=%1").arg(selectCharWnd->GetCurrentPageCount()));
    imageRevisionParts.push_back(QStringLiteral("selected=%1").arg(selectCharWnd->GetSelectedSlotNumber()));
    for (int visibleIndex = 0; visibleIndex < 3; ++visibleIndex) {
        UISelectCharWnd::VisibleSlotDisplay slotInfo{};
        if (!selectCharWnd->GetVisibleSlotDisplay(visibleIndex, &slotInfo)) {
            continue;
        }

        imageRevisionParts.push_back(QStringLiteral("slot=%1:%2:%3:%4:%5")
            .arg(slotInfo.slotNumber)
            .arg(slotInfo.occupied ? 1 : 0)
            .arg(slotInfo.selected ? 1 : 0)
            .arg(ToQString(slotInfo.name))
            .arg(slotInfo.level));

        QVariantMap slot;
        slot.insert(QStringLiteral("occupied"), slotInfo.occupied);
        slot.insert(QStringLiteral("selected"), slotInfo.selected);
        slot.insert(QStringLiteral("slotNumber"), slotInfo.slotNumber);
        slot.insert(QStringLiteral("x"), slotInfo.x);
        slot.insert(QStringLiteral("y"), slotInfo.y);
        slot.insert(QStringLiteral("width"), slotInfo.width);
        slot.insert(QStringLiteral("height"), slotInfo.height);
        slot.insert(QStringLiteral("name"), ToQString(slotInfo.name));
        slot.insert(QStringLiteral("job"), ToQString(slotInfo.job));
        slot.insert(QStringLiteral("level"), slotInfo.level);
        slot.insert(QStringLiteral("displayName"),
            slotInfo.occupied ? ToQString(slotInfo.name) : QStringLiteral("Empty Slot"));
        slot.insert(QStringLiteral("levelText"),
            slotInfo.occupied ? FormatCharacterSlotLevelText(slotInfo.level) : QString());
        slotList.push_back(slot);
    }
    state->setCharSelectSlots(slotList);

    UISelectCharWnd::SelectedCharacterDisplay selected{};
    QVariantMap details;
    if (selectCharWnd->GetSelectedCharacterDisplay(&selected) && selected.valid) {
        details.insert(QStringLiteral("name"), ToQString(selected.name));
        details.insert(QStringLiteral("job"), ToQString(selected.job));
        details.insert(QStringLiteral("level"), selected.level);
        details.insert(QStringLiteral("exp"), static_cast<qulonglong>(selected.exp));
        details.insert(QStringLiteral("hp"), selected.hp);
        details.insert(QStringLiteral("sp"), selected.sp);
        details.insert(QStringLiteral("str"), selected.str);
        details.insert(QStringLiteral("agi"), selected.agi);
        details.insert(QStringLiteral("vit"), selected.vit);
        details.insert(QStringLiteral("int"), selected.intStat);
        details.insert(QStringLiteral("dex"), selected.dex);
        details.insert(QStringLiteral("luk"), selected.luk);

        QVariantList fields;
        fields.reserve(selectCharWnd->GetQtSelectedDetailFieldCount());
        for (int index = 0; index < selectCharWnd->GetQtSelectedDetailFieldCount(); ++index) {
            UISelectCharWnd::QtDetailFieldDisplay display{};
            if (!selectCharWnd->GetQtSelectedDetailFieldDisplayForQt(index, &display)) {
                continue;
            }

            QVariantMap field;
            field.insert(QStringLiteral("x"), display.x);
            field.insert(QStringLiteral("y"), display.y);
            field.insert(QStringLiteral("width"), display.width);
            field.insert(QStringLiteral("height"), display.height);
            field.insert(QStringLiteral("text"), ToQString(display.text));
            fields.push_back(field);
        }
        details.insert(QStringLiteral("fields"), fields);
    }
    details.insert(QStringLiteral("imageRevision"), imageRevisionParts.join(QLatin1Char('|')));
    state->setCharSelectSelectedDetails(details);

    QVariantList pageButtons;
    pageButtons.reserve(selectCharWnd->GetQtPageButtonCount());
    for (int index = 0; index < selectCharWnd->GetQtPageButtonCount(); ++index) {
        UISelectCharWnd::QtButtonDisplay display{};
        if (!selectCharWnd->GetQtPageButtonDisplayForQt(index, &display)) {
            continue;
        }

        QVariantMap button;
        button.insert(QStringLiteral("id"), display.id);
        button.insert(QStringLiteral("x"), display.x);
        button.insert(QStringLiteral("y"), display.y);
        button.insert(QStringLiteral("width"), display.width);
        button.insert(QStringLiteral("height"), display.height);
        button.insert(QStringLiteral("label"), ToQString(display.label));
        button.insert(QStringLiteral("visible"), display.visible);
        pageButtons.push_back(button);
    }
    state->setCharSelectPageButtons(pageButtons);

    QVariantList actionButtons;
    actionButtons.reserve(selectCharWnd->GetQtActionButtonCount());
    for (int index = 0; index < selectCharWnd->GetQtActionButtonCount(); ++index) {
        UISelectCharWnd::QtButtonDisplay display{};
        if (!selectCharWnd->GetQtActionButtonDisplayForQt(index, &display)) {
            continue;
        }

        QVariantMap button;
        button.insert(QStringLiteral("id"), display.id);
        button.insert(QStringLiteral("x"), display.x);
        button.insert(QStringLiteral("y"), display.y);
        button.insert(QStringLiteral("width"), display.width);
        button.insert(QStringLiteral("height"), display.height);
        button.insert(QStringLiteral("label"), ToQString(display.label));
        button.insert(QStringLiteral("visible"), display.visible);
        actionButtons.push_back(button);
    }
    state->setCharSelectActionButtons(actionButtons);
}

void PopulateMakeCharState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIMakeCharWnd* const makeCharWnd = g_windowMgr.m_makeCharWnd;
    const bool visible = makeCharWnd && makeCharWnd->m_show != 0;
    state->setMakeCharVisible(visible);
    if (!visible) {
        state->setMakeCharPanelGeometry(0, 0, 0, 0);
        state->setMakeCharData(QString(), false, QVariantList{}, 0, 0);
        state->setMakeCharPanelData(QVariantMap{});
        state->setMakeCharButtons(QVariantList{});
        state->setMakeCharStatFields(QVariantList{});
        return;
    }

    g_windowMgr.m_makeCharWnd->EnsureQtLayout();
    state->setMakeCharPanelGeometry(
        makeCharWnd->m_x,
        makeCharWnd->m_y,
        makeCharWnd->m_w,
        makeCharWnd->m_h);

    UIMakeCharWnd::MakeCharDisplay display{};
    QVariantList stats;
    if (makeCharWnd->GetMakeCharDisplay(&display)) {
        for (int i = 0; i < 6; ++i) {
            stats.push_back(display.stats[i]);
        }
        state->setMakeCharData(
            ToQString(display.name),
            display.nameFocused,
            stats,
            display.hairIndex,
            display.hairColor);
        QVariantMap makeCharPanelData;
        makeCharPanelData.insert(QStringLiteral("imageRevision"), makeCharWnd->m_stateCnt);
        makeCharPanelData.insert(QStringLiteral("previewTitle"), QStringLiteral("Preview"));
        makeCharPanelData.insert(QStringLiteral("hairText"), QStringLiteral("Hair %1").arg(display.hairIndex));
        makeCharPanelData.insert(QStringLiteral("colorText"), QStringLiteral("Color %1").arg(display.hairColor));
        state->setMakeCharPanelData(makeCharPanelData);

        QVariantList statFields;
        statFields.reserve(makeCharWnd->GetQtStatFieldCount());
        for (int index = 0; index < makeCharWnd->GetQtStatFieldCount(); ++index) {
            UIMakeCharWnd::QtStatFieldDisplay fieldDisplay{};
            if (!makeCharWnd->GetQtStatFieldDisplayForQt(index, &fieldDisplay)) {
                continue;
            }

            QVariantMap field;
            field.insert(QStringLiteral("x"), fieldDisplay.x);
            field.insert(QStringLiteral("y"), fieldDisplay.y);
            field.insert(QStringLiteral("width"), fieldDisplay.width);
            field.insert(QStringLiteral("height"), fieldDisplay.height);
            field.insert(QStringLiteral("label"), ToQString(fieldDisplay.label));
            field.insert(QStringLiteral("value"), fieldDisplay.value);
            statFields.push_back(field);
        }
        state->setMakeCharStatFields(statFields);

        QVariantList buttons;
        buttons.reserve(makeCharWnd->GetQtButtonCount());
        for (int index = 0; index < makeCharWnd->GetQtButtonCount(); ++index) {
            UIMakeCharWnd::QtButtonDisplay buttonDisplay{};
            if (!makeCharWnd->GetQtButtonDisplayForQt(index, &buttonDisplay)) {
                continue;
            }

            QVariantMap button;
            button.insert(QStringLiteral("id"), buttonDisplay.id);
            button.insert(QStringLiteral("x"), buttonDisplay.x);
            button.insert(QStringLiteral("y"), buttonDisplay.y);
            button.insert(QStringLiteral("width"), buttonDisplay.width);
            button.insert(QStringLiteral("height"), buttonDisplay.height);
            button.insert(QStringLiteral("pressed"), buttonDisplay.pressed);
            button.insert(QStringLiteral("label"), ToQString(buttonDisplay.label));
            buttons.push_back(button);
        }
        state->setMakeCharButtons(buttons);
        return;
    }

    state->setMakeCharData(QString(), false, QVariantList{}, 0, 0);
    state->setMakeCharPanelData(QVariantMap{});
    state->setMakeCharButtons(QVariantList{});
    state->setMakeCharStatFields(QVariantList{});
}

void PopulateNotificationState(QtUiState* state)
{
    if (!state) {
        return;
    }

    QVariantList notifications;
    const UINotifyLevelUpWnd* notices[] = {
        g_windowMgr.m_notifyLevelUpWnd,
        g_windowMgr.m_notifyJobLevelUpWnd,
    };

    for (const UINotifyLevelUpWnd* notice : notices) {
        if (!notice || notice->m_show == 0) {
            continue;
        }

        QVariantMap entry;
        entry.insert(QStringLiteral("x"), notice->m_x);
        entry.insert(QStringLiteral("y"), notice->m_y);
        entry.insert(QStringLiteral("width"), notice->m_w);
        entry.insert(QStringLiteral("height"), notice->m_h);
        entry.insert(QStringLiteral("title"),
            notice->IsJobLevelNotice() ? QStringLiteral("Job Level Up") : QStringLiteral("Level Up"));
        entry.insert(QStringLiteral("accent"),
            notice->IsJobLevelNotice() ? QStringLiteral("#4f8bd6") : QStringLiteral("#7ca93d"));
        notifications.push_back(entry);
    }

    state->setNotifications(notifications);
}

void PopulateShopChoiceState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIChooseSellBuyWnd* const shopWnd = g_windowMgr.m_chooseSellBuyWnd;
    const bool visible = shopWnd && shopWnd->m_show != 0;
    state->setShopChoiceVisible(visible);
    if (!visible) {
        state->setShopChoiceGeometry(0, 0, 0, 0);
        state->setShopChoiceText(QString(), QString());
        state->setShopChoiceButtons(QVariantList{});
        return;
    }

    state->setShopChoiceGeometry(shopWnd->m_x, shopWnd->m_y, shopWnd->m_w, shopWnd->m_h);
    state->setShopChoiceText(QStringLiteral("Shop"), QStringLiteral("Choose a transaction type."));

    QVariantList buttons;
    struct ButtonSpec {
        UIChooseSellBuyWnd::ButtonId id;
        const char* label;
        int offsetX;
        int offsetY;
        int width;
        int height;
    };
    const ButtonSpec specs[] = {
        { UIChooseSellBuyWnd::ButtonBuy, "Buy", 14, 46, 76, 22 },
        { UIChooseSellBuyWnd::ButtonSell, "Sell", 90, 46, 76, 22 },
        { UIChooseSellBuyWnd::ButtonCancel, "Cancel", 52, 74, 76, 22 },
    };
    for (const ButtonSpec& spec : specs) {
        QVariantMap button;
        button.insert(QStringLiteral("label"), ToQString(spec.label));
        button.insert(QStringLiteral("x"), shopWnd->m_x + spec.offsetX);
        button.insert(QStringLiteral("y"), shopWnd->m_y + spec.offsetY);
        button.insert(QStringLiteral("width"), spec.width);
        button.insert(QStringLiteral("height"), spec.height);
        button.insert(QStringLiteral("hot"), shopWnd->GetHoverButton() == spec.id);
        button.insert(QStringLiteral("pressed"), shopWnd->GetPressedButton() == spec.id);
        buttons.push_back(button);
    }
    state->setShopChoiceButtons(buttons);
}

void PopulateNpcMenuState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UINpcMenuWnd* const menuWnd = g_windowMgr.m_npcMenuWnd;
    const bool visible = menuWnd && menuWnd->m_show != 0;
    state->setNpcMenuVisible(visible);
    if (!visible) {
        state->setNpcMenuGeometry(0, 0, 0, 0);
        state->setNpcMenuSelection(-1);
        state->setNpcMenuHoverIndex(-1);
        state->setNpcMenuButtons(false, false);
        state->setNpcMenuOptions(QVariantList{});
        state->setNpcMenuButtonsData(QVariantList{});
        return;
    }

    state->setNpcMenuGeometry(menuWnd->m_x, menuWnd->m_y, menuWnd->m_w, menuWnd->m_h);
    state->setNpcMenuSelection(menuWnd->GetSelectedIndex());
    state->setNpcMenuHoverIndex(menuWnd->GetHoverIndex());
    state->setNpcMenuButtons(menuWnd->IsOkPressed(), menuWnd->IsCancelPressed());

    QVariantList options;
    const std::vector<std::string>& rawOptions = menuWnd->GetOptions();
    options.reserve(static_cast<qsizetype>(rawOptions.size()));
    for (const std::string& option : rawOptions) {
        options.push_back(NpcColorCodesToHtml(option));
    }
    state->setNpcMenuOptions(options);

    QVariantList buttons;
    RECT rect{};
    if (menuWnd->GetOkRectForQt(&rect)) {
        QVariantMap button;
        button.insert(QStringLiteral("x"), static_cast<int>(rect.left));
        button.insert(QStringLiteral("y"), static_cast<int>(rect.top));
        button.insert(QStringLiteral("width"), static_cast<int>(rect.right - rect.left));
        button.insert(QStringLiteral("height"), static_cast<int>(rect.bottom - rect.top));
        button.insert(QStringLiteral("label"), QStringLiteral("OK"));
        button.insert(QStringLiteral("pressed"), menuWnd->IsOkPressed());
        buttons.push_back(button);
    }
    if (menuWnd->GetCancelRectForQt(&rect)) {
        QVariantMap button;
        button.insert(QStringLiteral("x"), static_cast<int>(rect.left));
        button.insert(QStringLiteral("y"), static_cast<int>(rect.top));
        button.insert(QStringLiteral("width"), static_cast<int>(rect.right - rect.left));
        button.insert(QStringLiteral("height"), static_cast<int>(rect.bottom - rect.top));
        button.insert(QStringLiteral("label"), QStringLiteral("Cancel"));
        button.insert(QStringLiteral("pressed"), menuWnd->IsCancelPressed());
        buttons.push_back(button);
    }
    state->setNpcMenuButtonsData(buttons);
}

void PopulateSayDialogState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UISayDialogWnd* const dialogWnd = g_windowMgr.m_sayDialogWnd;
    const bool visible = dialogWnd && dialogWnd->m_show != 0;
    state->setSayDialogVisible(visible);
    if (!visible) {
        state->setSayDialogGeometry(0, 0, 0, 0);
        state->setSayDialogText(QString());
        state->setSayDialogAction(false, QString(), false, false);
        state->setSayDialogActionButton(QVariantMap{});
        return;
    }

    state->setSayDialogGeometry(dialogWnd->m_x, dialogWnd->m_y, dialogWnd->m_w, dialogWnd->m_h);
    state->setSayDialogText(NpcColorCodesToHtml(dialogWnd->GetDisplayText()));
    state->setSayDialogAction(
        dialogWnd->HasActionButton(),
        dialogWnd->IsNextAction() ? QStringLiteral("Next") : QStringLiteral("Close"),
        dialogWnd->IsHoveringAction(),
        dialogWnd->IsPressingAction());
    QVariantMap actionButton;
    RECT actionRect{};
    if (dialogWnd->GetActionRectForQt(&actionRect)) {
        actionButton.insert(QStringLiteral("x"), static_cast<int>(actionRect.left));
        actionButton.insert(QStringLiteral("y"), static_cast<int>(actionRect.top));
        actionButton.insert(QStringLiteral("width"), static_cast<int>(actionRect.right - actionRect.left));
        actionButton.insert(QStringLiteral("height"), static_cast<int>(actionRect.bottom - actionRect.top));
        actionButton.insert(QStringLiteral("label"), dialogWnd->IsNextAction() ? QStringLiteral("Next") : QStringLiteral("Close"));
        actionButton.insert(QStringLiteral("hovered"), dialogWnd->IsHoveringAction());
        actionButton.insert(QStringLiteral("pressed"), dialogWnd->IsPressingAction());
        actionButton.insert(QStringLiteral("visible"), dialogWnd->HasActionButton());
    }
    state->setSayDialogActionButton(actionButton);
}

void PopulateNpcInputState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UINpcInputWnd* const inputWnd = g_windowMgr.m_npcInputWnd;
    const bool visible = inputWnd && inputWnd->m_show != 0;
    state->setNpcInputVisible(visible);
    if (!visible) {
        state->setNpcInputGeometry(0, 0, 0, 0);
        state->setNpcInputText(QString(), QString());
        state->setNpcInputButtons(false, false);
        state->setNpcInputButtonsData(QVariantList{});
        return;
    }

    state->setNpcInputGeometry(inputWnd->m_x, inputWnd->m_y, inputWnd->m_w, inputWnd->m_h);
    state->setNpcInputText(
        inputWnd->GetInputMode() == UINpcInputWnd::InputMode::Number
            ? QStringLiteral("Enter a number")
            : QStringLiteral("Enter text"),
        ToQString(inputWnd->GetInputText()));
    state->setNpcInputButtons(inputWnd->IsOkPressed(), inputWnd->IsCancelPressed());

    QVariantList buttons;
    RECT rect{};
    if (inputWnd->GetOkRectForQt(&rect)) {
        QVariantMap button;
        button.insert(QStringLiteral("x"), static_cast<int>(rect.left));
        button.insert(QStringLiteral("y"), static_cast<int>(rect.top));
        button.insert(QStringLiteral("width"), static_cast<int>(rect.right - rect.left));
        button.insert(QStringLiteral("height"), static_cast<int>(rect.bottom - rect.top));
        button.insert(QStringLiteral("label"), QStringLiteral("OK"));
        button.insert(QStringLiteral("pressed"), inputWnd->IsOkPressed());
        buttons.push_back(button);
    }
    if (inputWnd->GetCancelRectForQt(&rect)) {
        QVariantMap button;
        button.insert(QStringLiteral("x"), static_cast<int>(rect.left));
        button.insert(QStringLiteral("y"), static_cast<int>(rect.top));
        button.insert(QStringLiteral("width"), static_cast<int>(rect.right - rect.left));
        button.insert(QStringLiteral("height"), static_cast<int>(rect.bottom - rect.top));
        button.insert(QStringLiteral("label"), QStringLiteral("Cancel"));
        button.insert(QStringLiteral("pressed"), inputWnd->IsCancelPressed());
        buttons.push_back(button);
    }
    state->setNpcInputButtonsData(buttons);
}

void PopulateChooseMenuState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIChooseWnd* const chooseWnd = g_windowMgr.m_chooseWnd;
    const bool visible = chooseWnd && chooseWnd->m_show != 0;
    state->setChooseMenuVisible(visible);
    if (!visible) {
        state->setChooseMenuGeometry(0, 0, 0, 0);
        state->setChooseMenuSelectedIndex(-1);
        state->setChooseMenuPressedIndex(-1);
        state->setChooseMenuOptions(QVariantList{});
        return;
    }

    state->setChooseMenuGeometry(chooseWnd->m_x, chooseWnd->m_y, chooseWnd->m_w, chooseWnd->m_h);
    state->setChooseMenuSelectedIndex(chooseWnd->GetSelectedIndex());
    state->setChooseMenuPressedIndex(chooseWnd->GetPressedIndex());

    QVariantList options;
    const int optionCount = chooseWnd->GetEntryCount();
    options.reserve(optionCount);
    for (int index = 0; index < optionCount; ++index) {
        options.push_back(ToQString(chooseWnd->GetEntryLabelForQt(index)));
    }
    state->setChooseMenuOptions(options);
}

void PopulateItemShopState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIItemShopWnd* const shopWnd = g_windowMgr.m_itemShopWnd;
    const bool visible = shopWnd && shopWnd->m_show != 0;
    state->setItemShopVisible(visible);
    if (!visible) {
        state->setItemShopGeometry(0, 0, 0, 0);
        state->setItemShopTitle(QString());
        state->setItemShopData(QVariantMap{});
        state->setItemShopRows(QVariantList{});
        return;
    }

    state->setItemShopGeometry(shopWnd->m_x, shopWnd->m_y, shopWnd->m_w, shopWnd->m_h);
    const bool sellMode = g_session.m_shopMode == NpcShopMode::Sell;
    const QString shopTitle = sellMode
        ? QStringLiteral("Sellable Items")
        : QStringLiteral("Shop Items");
    state->setItemShopTitle(shopTitle);

    QVariantMap shopData;
    shopData.insert(QStringLiteral("title"), shopTitle);
    shopData.insert(QStringLiteral("nameLabel"), QStringLiteral("Item"));
    shopData.insert(QStringLiteral("quantityLabel"), QStringLiteral("Qty"));
    shopData.insert(QStringLiteral("priceLabel"), QStringLiteral("Price"));
    shopData.insert(QStringLiteral("showQuantity"), sellMode);
    state->setItemShopData(shopData);

    QVariantList rows;
    const int startRow = shopWnd->GetViewOffset();
    const int endRow = (std::min)(
        static_cast<int>(g_session.m_shopRows.size()),
        startRow + shopWnd->GetVisibleRowCountForQt());
    rows.reserve((std::max)(0, endRow - startRow));
    for (int rowIndex = startRow; rowIndex < endRow; ++rowIndex) {
        const NPC_SHOP_ROW& row = g_session.m_shopRows[static_cast<size_t>(rowIndex)];
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), ToQString(shopui::GetItemDisplayName(row.itemInfo)));
        entry.insert(QStringLiteral("quantity"), row.availableCount);
        entry.insert(QStringLiteral("price"), g_session.GetNpcShopUnitPrice(row));
        entry.insert(QStringLiteral("selected"), g_session.m_shopSelectedSourceRow == rowIndex);
        entry.insert(QStringLiteral("hover"), shopWnd->GetHoverRow() == rowIndex);
        rows.push_back(entry);
    }
    state->setItemShopRows(rows);
}

void PopulateItemPurchaseState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIItemPurchaseWnd* const purchaseWnd = g_windowMgr.m_itemPurchaseWnd;
    const bool visible = purchaseWnd && purchaseWnd->m_show != 0;
    state->setItemPurchaseVisible(visible);
    if (!visible) {
        state->setItemPurchaseGeometry(0, 0, 0, 0);
        state->setItemPurchaseTotal(0);
        state->setItemPurchaseData(QVariantMap{});
        state->setItemPurchaseRows(QVariantList{});
        state->setItemPurchaseButtons(QVariantList{});
        return;
    }

    state->setItemPurchaseGeometry(purchaseWnd->m_x, purchaseWnd->m_y, purchaseWnd->m_w, purchaseWnd->m_h);
    state->setItemPurchaseTotal(g_session.m_shopDealTotal);
    QVariantMap purchaseData;
    purchaseData.insert(QStringLiteral("title"), QStringLiteral("Purchase"));
    purchaseData.insert(QStringLiteral("nameLabel"), QStringLiteral("Item"));
    purchaseData.insert(QStringLiteral("quantityLabel"), QStringLiteral("Qty"));
    purchaseData.insert(QStringLiteral("amountLabel"), QStringLiteral("Cost"));
    purchaseData.insert(QStringLiteral("totalLabel"), QStringLiteral("Total"));
    state->setItemPurchaseData(purchaseData);

    QVariantList rows;
    const int startRow = purchaseWnd->GetViewOffset();
    const int endRow = (std::min)(
        static_cast<int>(g_session.m_shopDealRows.size()),
        startRow + purchaseWnd->GetVisibleRowCountForQt() - 1);
    rows.reserve((std::max)(0, endRow - startRow));
    for (int rowIndex = startRow; rowIndex < endRow; ++rowIndex) {
        const NPC_SHOP_DEAL_ROW& row = g_session.m_shopDealRows[static_cast<size_t>(rowIndex)];
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), ToQString(shopui::GetItemDisplayName(row.itemInfo)));
        entry.insert(QStringLiteral("quantity"), row.quantity);
        entry.insert(QStringLiteral("cost"), row.unitPrice * row.quantity);
        entry.insert(QStringLiteral("selected"), g_session.m_shopSelectedDealRow == rowIndex);
        entry.insert(QStringLiteral("hover"), purchaseWnd->GetHoverRow() == rowIndex);
        rows.push_back(entry);
    }
    state->setItemPurchaseRows(rows);

    QVariantList buttons;
    struct ButtonSpec {
        int id;
        const char* label;
    };
    const ButtonSpec specs[] = {
        { 0, "Add" },
        { 1, "Remove" },
        { 2, "Buy" },
        { 3, "Cancel" },
    };
    for (const ButtonSpec& spec : specs) {
        QVariantMap button;
        RECT rect{};
        if (purchaseWnd->GetButtonRectForQt(spec.id, &rect)) {
            button.insert(QStringLiteral("x"), static_cast<int>(rect.left));
            button.insert(QStringLiteral("y"), static_cast<int>(rect.top));
            button.insert(QStringLiteral("width"), static_cast<int>(rect.right - rect.left));
            button.insert(QStringLiteral("height"), static_cast<int>(rect.bottom - rect.top));
        }
        button.insert(QStringLiteral("label"), ToQString(spec.label));
        button.insert(QStringLiteral("hot"), purchaseWnd->GetHoverButton() == spec.id);
        button.insert(QStringLiteral("pressed"), purchaseWnd->GetPressedButton() == spec.id);
        buttons.push_back(button);
    }
    state->setItemPurchaseButtons(buttons);
}

void PopulateItemSellState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIItemSellWnd* const sellWnd = g_windowMgr.m_itemSellWnd;
    const bool visible = sellWnd && sellWnd->m_show != 0;
    state->setItemSellVisible(visible);
    if (!visible) {
        state->setItemSellGeometry(0, 0, 0, 0);
        state->setItemSellTotal(0);
        state->setItemSellData(QVariantMap{});
        state->setItemSellRows(QVariantList{});
        state->setItemSellButtons(QVariantList{});
        return;
    }

    state->setItemSellGeometry(sellWnd->m_x, sellWnd->m_y, sellWnd->m_w, sellWnd->m_h);
    state->setItemSellTotal(g_session.m_shopDealTotal);
    QVariantMap sellData;
    sellData.insert(QStringLiteral("title"), QStringLiteral("Sell"));
    sellData.insert(QStringLiteral("nameLabel"), QStringLiteral("Item"));
    sellData.insert(QStringLiteral("quantityLabel"), QStringLiteral("Qty"));
    sellData.insert(QStringLiteral("amountLabel"), QStringLiteral("Gain"));
    sellData.insert(QStringLiteral("totalLabel"), QStringLiteral("Total"));
    state->setItemSellData(sellData);

    QVariantList rows;
    const int startRow = sellWnd->GetViewOffset();
    const int endRow = (std::min)(
        static_cast<int>(g_session.m_shopDealRows.size()),
        startRow + sellWnd->GetVisibleRowCountForQt() - 1);
    rows.reserve((std::max)(0, endRow - startRow));
    for (int rowIndex = startRow; rowIndex < endRow; ++rowIndex) {
        const NPC_SHOP_DEAL_ROW& row = g_session.m_shopDealRows[static_cast<size_t>(rowIndex)];
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), ToQString(shopui::GetItemDisplayName(row.itemInfo)));
        entry.insert(QStringLiteral("quantity"), row.quantity);
        entry.insert(QStringLiteral("gain"), row.unitPrice * row.quantity);
        entry.insert(QStringLiteral("selected"), g_session.m_shopSelectedDealRow == rowIndex);
        entry.insert(QStringLiteral("hover"), sellWnd->GetHoverRow() == rowIndex);
        rows.push_back(entry);
    }
    state->setItemSellRows(rows);

    QVariantList buttons;
    struct ButtonSpec {
        int id;
        const char* label;
    };
    const ButtonSpec specs[] = {
        { 0, "Add" },
        { 1, "Remove" },
        { 2, "Sell" },
        { 3, "Cancel" },
    };
    for (const ButtonSpec& spec : specs) {
        QVariantMap button;
        RECT rect{};
        if (sellWnd->GetButtonRectForQt(spec.id, &rect)) {
            button.insert(QStringLiteral("x"), static_cast<int>(rect.left));
            button.insert(QStringLiteral("y"), static_cast<int>(rect.top));
            button.insert(QStringLiteral("width"), static_cast<int>(rect.right - rect.left));
            button.insert(QStringLiteral("height"), static_cast<int>(rect.bottom - rect.top));
        }
        button.insert(QStringLiteral("label"), ToQString(spec.label));
        button.insert(QStringLiteral("hot"), sellWnd->GetHoverButton() == spec.id);
        button.insert(QStringLiteral("pressed"), sellWnd->GetPressedButton() == spec.id);
        buttons.push_back(button);
    }
    state->setItemSellButtons(buttons);
}

void PopulateShortCutState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIShortCutWnd* const shortCutWnd = g_windowMgr.m_shortCutWnd;
    const bool visible = shortCutWnd && shortCutWnd->m_show != 0;
    state->setShortCutVisible(visible);
    if (!visible) {
        state->setShortCutGeometry(0, 0, 0, 0);
        state->setShortCutPage(0);
        state->setShortCutHoverSlot(-1);
        state->setShortCutSlots(QVariantList{});
        return;
    }

    state->setShortCutGeometry(shortCutWnd->m_x, shortCutWnd->m_y, shortCutWnd->m_w, shortCutWnd->m_h);
    state->setShortCutPage(g_session.GetShortcutPage() + 1);
    state->setShortCutHoverSlot(shortCutWnd->GetHoverSlot());

    QVariantList shortcutSlots;
    shortcutSlots.reserve(kShortcutSlotsPerPage);
    for (int slotIndex = 0; slotIndex < kShortcutSlotsPerPage; ++slotIndex) {
        QVariantMap slotEntry;
        slotEntry.insert(QStringLiteral("index"), slotIndex);
        slotEntry.insert(QStringLiteral("hover"), shortCutWnd->GetHoverSlot() == slotIndex);

        const SHORTCUT_SLOT* slot = g_session.GetShortcutSlotByVisibleIndex(slotIndex);
        if (slot && slot->id != 0) {
            slotEntry.insert(QStringLiteral("occupied"), true);
            slotEntry.insert(QStringLiteral("isSkill"), slot->isSkill != 0);
            slotEntry.insert(QStringLiteral("skillId"), slot->isSkill != 0 ? static_cast<int>(slot->id) : 0);
            slotEntry.insert(QStringLiteral("itemId"), slot->isSkill != 0 ? 0u : slot->id);
            slotEntry.insert(QStringLiteral("label"), ResolveShortcutSlotLabel(slot));
            slotEntry.insert(QStringLiteral("count"), static_cast<int>(slot->count));
        } else {
            slotEntry.insert(QStringLiteral("occupied"), false);
            slotEntry.insert(QStringLiteral("isSkill"), false);
            slotEntry.insert(QStringLiteral("skillId"), 0);
            slotEntry.insert(QStringLiteral("itemId"), 0u);
            slotEntry.insert(QStringLiteral("label"), QString());
            slotEntry.insert(QStringLiteral("count"), 0);
        }
        shortcutSlots.push_back(slotEntry);
    }
    state->setShortCutSlots(shortcutSlots);
}

void PopulateBasicInfoState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIBasicInfoWnd* const basicInfoWnd = g_windowMgr.m_basicInfoWnd;
    const bool visible = basicInfoWnd && basicInfoWnd->m_show != 0;
    state->setBasicInfoVisible(visible);
    if (!visible) {
        state->setBasicInfoGeometry(0, 0, 0, 0);
        state->setBasicInfoMini(false);
        state->setBasicInfoData(QVariantMap{});
        return;
    }

    state->setBasicInfoGeometry(basicInfoWnd->m_x, basicInfoWnd->m_y, basicInfoWnd->m_w, basicInfoWnd->m_h);
    state->setBasicInfoMini(basicInfoWnd->IsMiniMode());

    UIBasicInfoWnd::DisplayData display{};
    QVariantMap data;
    if (basicInfoWnd->GetDisplayDataForQt(&display)) {
        const QString jobName = ToQString(display.jobName);
        data.insert(QStringLiteral("name"), ToQString(display.name));
        data.insert(QStringLiteral("jobName"), jobName);
        data.insert(QStringLiteral("level"), display.level);
        data.insert(QStringLiteral("jobLevel"), display.jobLevel);
        data.insert(QStringLiteral("hp"), display.hp);
        data.insert(QStringLiteral("maxHp"), display.maxHp);
        data.insert(QStringLiteral("sp"), display.sp);
        data.insert(QStringLiteral("maxSp"), display.maxSp);
        data.insert(QStringLiteral("money"), display.money);
        data.insert(QStringLiteral("weight"), display.weight);
        data.insert(QStringLiteral("maxWeight"), display.maxWeight);
        data.insert(QStringLiteral("expPercent"), display.expPercent);
        data.insert(QStringLiteral("jobExpPercent"), display.jobExpPercent);
        data.insert(QStringLiteral("miniHeaderText"), FormatBasicInfoMiniHeaderText(display.level, jobName, display.expPercent));
        data.insert(QStringLiteral("miniStatusText"),
            FormatBasicInfoMiniStatusText(display.hp, display.maxHp, display.sp, display.maxSp, display.money));
        data.insert(QStringLiteral("hpText"), FormatBasicGaugeText("HP", display.hp, display.maxHp));
        data.insert(QStringLiteral("spText"), FormatBasicGaugeText("SP", display.sp, display.maxSp));
        data.insert(QStringLiteral("baseLevelText"), FormatBasicLevelText("Base Lv.", display.level));
        data.insert(QStringLiteral("jobLevelText"), FormatBasicLevelText("Job Lv.", display.jobLevel));
        data.insert(QStringLiteral("weightText"),
            QStringLiteral("Weight : %1 / %2").arg(display.weight).arg(display.maxWeight));
        data.insert(QStringLiteral("moneyText"),
            QStringLiteral("Zeny : %1").arg(display.money));

        QVariantList systemButtons;
        systemButtons.reserve(basicInfoWnd->GetQtSystemButtonCount());
        for (int index = 0; index < basicInfoWnd->GetQtSystemButtonCount(); ++index) {
            UIBasicInfoWnd::QtButtonDisplay buttonDisplay{};
            if (!basicInfoWnd->GetQtSystemButtonDisplayForQt(index, &buttonDisplay)) {
                continue;
            }

            QVariantMap button;
            button.insert(QStringLiteral("id"), buttonDisplay.id);
            button.insert(QStringLiteral("x"), buttonDisplay.x);
            button.insert(QStringLiteral("y"), buttonDisplay.y);
            button.insert(QStringLiteral("width"), buttonDisplay.width);
            button.insert(QStringLiteral("height"), buttonDisplay.height);
            button.insert(QStringLiteral("label"), ToQString(buttonDisplay.label));
            button.insert(QStringLiteral("visible"), buttonDisplay.visible);
            systemButtons.push_back(button);
        }
        data.insert(QStringLiteral("systemButtons"), systemButtons);

        QVariantList menuButtons;
        menuButtons.reserve(basicInfoWnd->GetQtMenuButtonCount());
        for (int index = 0; index < basicInfoWnd->GetQtMenuButtonCount(); ++index) {
            UIBasicInfoWnd::QtButtonDisplay buttonDisplay{};
            if (!basicInfoWnd->GetQtMenuButtonDisplayForQt(index, &buttonDisplay)) {
                continue;
            }

            QVariantMap button;
            button.insert(QStringLiteral("id"), buttonDisplay.id);
            button.insert(QStringLiteral("x"), buttonDisplay.x);
            button.insert(QStringLiteral("y"), buttonDisplay.y);
            button.insert(QStringLiteral("width"), buttonDisplay.width);
            button.insert(QStringLiteral("height"), buttonDisplay.height);
            button.insert(QStringLiteral("label"), ToQString(buttonDisplay.label));
            button.insert(QStringLiteral("visible"), buttonDisplay.visible);
            menuButtons.push_back(button);
        }
        data.insert(QStringLiteral("menuButtons"), menuButtons);
    }
    state->setBasicInfoData(data);
}

void PopulateStatusState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIStatusWnd* const statusWnd = g_windowMgr.m_statusWnd;
    const bool visible = statusWnd && statusWnd->m_show != 0;
    state->setStatusVisible(visible);
    if (!visible) {
        state->setStatusGeometry(0, 0, 0, 0);
        state->setStatusMini(false);
        state->setStatusPage(0);
        state->setStatusData(QVariantMap{});
        return;
    }

    state->setStatusGeometry(statusWnd->m_x, statusWnd->m_y, statusWnd->m_w, statusWnd->m_h);
    state->setStatusMini(statusWnd->IsMiniMode());
    state->setStatusPage(statusWnd->GetPageForQt());

    UIStatusWnd::DisplayData display{};
    QVariantMap data;
    if (statusWnd->GetDisplayDataForQt(&display)) {
        static const char* const kLabels[] = { "STR", "AGI", "VIT", "INT", "DEX", "LUK" };

        QVariantList stats;
        stats.reserve(6);
        for (int index = 0; index < 6; ++index) {
            QVariantMap row;
            row.insert(QStringLiteral("label"), QString::fromLatin1(kLabels[index]));
            row.insert(QStringLiteral("value"), FormatBaseStatusText(display.baseStats[index], display.plusStats[index]));
            row.insert(QStringLiteral("cost"), display.statCosts[index]);
            UIStatusWnd::QtButtonDisplay incrementButton{};
            const bool hasIncrementButton = statusWnd->GetQtIncrementButtonDisplayForQt(index, &incrementButton);
            row.insert(QStringLiteral("canIncrease"), hasIncrementButton && incrementButton.visible);
            if (hasIncrementButton) {
                row.insert(QStringLiteral("increaseX"), incrementButton.x);
                row.insert(QStringLiteral("increaseY"), incrementButton.y);
                row.insert(QStringLiteral("increaseWidth"), incrementButton.width);
                row.insert(QStringLiteral("increaseHeight"), incrementButton.height);
                row.insert(QStringLiteral("increaseLabel"), ToQString(incrementButton.label));
            }
            stats.push_back(row);
        }

        data.insert(QStringLiteral("stats"), stats);

        QVariantList systemButtons;
        systemButtons.reserve(statusWnd->GetQtSystemButtonCount());
        for (int index = 0; index < statusWnd->GetQtSystemButtonCount(); ++index) {
            UIStatusWnd::QtButtonDisplay buttonDisplay{};
            if (!statusWnd->GetQtSystemButtonDisplayForQt(index, &buttonDisplay)) {
                continue;
            }

            QVariantMap button;
            button.insert(QStringLiteral("id"), buttonDisplay.id);
            button.insert(QStringLiteral("x"), buttonDisplay.x);
            button.insert(QStringLiteral("y"), buttonDisplay.y);
            button.insert(QStringLiteral("width"), buttonDisplay.width);
            button.insert(QStringLiteral("height"), buttonDisplay.height);
            button.insert(QStringLiteral("label"), ToQString(buttonDisplay.label));
            button.insert(QStringLiteral("visible"), buttonDisplay.visible);
            button.insert(QStringLiteral("active"), buttonDisplay.active);
            systemButtons.push_back(button);
        }
        data.insert(QStringLiteral("systemButtons"), systemButtons);

        QVariantList pageTabs;
        pageTabs.reserve(statusWnd->GetQtPageTabCount());
        for (int index = 0; index < statusWnd->GetQtPageTabCount(); ++index) {
            UIStatusWnd::QtButtonDisplay buttonDisplay{};
            if (!statusWnd->GetQtPageTabDisplayForQt(index, &buttonDisplay)) {
                continue;
            }

            QVariantMap button;
            button.insert(QStringLiteral("id"), buttonDisplay.id);
            button.insert(QStringLiteral("x"), buttonDisplay.x);
            button.insert(QStringLiteral("y"), buttonDisplay.y);
            button.insert(QStringLiteral("width"), buttonDisplay.width);
            button.insert(QStringLiteral("height"), buttonDisplay.height);
            button.insert(QStringLiteral("label"), ToQString(buttonDisplay.label));
            button.insert(QStringLiteral("visible"), buttonDisplay.visible);
            button.insert(QStringLiteral("active"), buttonDisplay.active);
            pageTabs.push_back(button);
        }
        data.insert(QStringLiteral("pageTabs"), pageTabs);

        data.insert(QStringLiteral("title"), QStringLiteral("Status"));
        data.insert(QStringLiteral("attackText"), FormatCompositeStatusText(display.attack, display.refineAttack));
        data.insert(QStringLiteral("matkText"), FormatMatkStatusText(display.matkMin, display.matkMax));
        data.insert(QStringLiteral("hit"), display.hit);
        data.insert(QStringLiteral("critical"), display.critical);
        data.insert(QStringLiteral("statusPoint"), display.statusPoint);
        data.insert(QStringLiteral("itemDefText"), FormatCompositeStatusText(display.itemDef, display.plusDef));
        data.insert(QStringLiteral("itemMdefText"), FormatCompositeStatusText(display.itemMdef, display.plusMdef));
        data.insert(QStringLiteral("fleeText"), FormatCompositeStatusText(display.flee, display.plusFlee));
        data.insert(QStringLiteral("aspdText"), FormatCompositeStatusText(display.aspd, display.plusAspd));
        data.insert(QStringLiteral("miniPointsText"), QStringLiteral("Points %1").arg(display.statusPoint));
        data.insert(QStringLiteral("attackLine"), QStringLiteral("Atk  %1").arg(data.value(QStringLiteral("attackText")).toString()));
        data.insert(QStringLiteral("matkLine"), QStringLiteral("Matk %1").arg(data.value(QStringLiteral("matkText")).toString()));
        data.insert(QStringLiteral("hitLine"), QStringLiteral("Hit   %1").arg(display.hit));
        data.insert(QStringLiteral("critLine"), QStringLiteral("Crit  %1").arg(display.critical));
        data.insert(QStringLiteral("pointsLine"), QStringLiteral("Pts %1").arg(display.statusPoint));
        data.insert(QStringLiteral("defLine"), QStringLiteral("Def   %1").arg(data.value(QStringLiteral("itemDefText")).toString()));
        data.insert(QStringLiteral("mdefLine"), QStringLiteral("Mdef %1").arg(data.value(QStringLiteral("itemMdefText")).toString()));
        data.insert(QStringLiteral("fleeLine"), QStringLiteral("Flee  %1").arg(data.value(QStringLiteral("fleeText")).toString()));
        data.insert(QStringLiteral("aspdLine"), QStringLiteral("Aspd %1").arg(data.value(QStringLiteral("aspdText")).toString()));
    }
    state->setStatusData(data);
}

void PopulateChatWindowState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UINewChatWnd* const chatWnd = g_windowMgr.m_chatWnd;
    const bool visible = chatWnd && chatWnd->m_show != 0;
    state->setChatWindowVisible(visible);
    if (!visible) {
        state->setChatWindowGeometry(0, 0, 0, 0);
        state->setChatWindowInputActive(false);
        state->setChatWindowInputText(QString());
        state->setChatWindowLines(QVariantList{});
        return;
    }

    state->setChatWindowGeometry(chatWnd->m_x, chatWnd->m_y, chatWnd->m_w, chatWnd->m_h);
    state->setChatWindowInputActive(chatWnd->IsInputActive());
    state->setChatWindowInputText(ToQString(chatWnd->GetInputText()));

    QVariantList lines;
    const std::vector<ChatLine>& visibleLines = chatWnd->GetVisibleLines();
    lines.reserve(static_cast<qsizetype>(visibleLines.size()));
    for (const ChatLine& line : visibleLines) {
        QVariantMap entry;
        entry.insert(QStringLiteral("text"), ToQString(line.text));
        entry.insert(QStringLiteral("color"),
            ToCssColor(
                static_cast<u8>((line.color >> 16) & 0xFFu),
                static_cast<u8>((line.color >> 8) & 0xFFu),
                static_cast<u8>(line.color & 0xFFu)));
        lines.push_back(entry);
    }
    state->setChatWindowLines(lines);
}

void PopulateRechargeGaugeState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIRechargeGage* rechargeGauge = nullptr;
    for (UIWindow* const window : g_windowMgr.m_children) {
        UIRechargeGage* const gauge = dynamic_cast<UIRechargeGage*>(window);
        if (!gauge || gauge->m_show == 0) {
            continue;
        }
        rechargeGauge = gauge;
        break;
    }

    const bool visible = rechargeGauge != nullptr;
    state->setRechargeGaugeVisible(visible);
    if (!visible) {
        state->setRechargeGaugeGeometry(0, 0, 0, 0);
        state->setRechargeGaugeProgress(0, 0);
        return;
    }

    state->setRechargeGaugeGeometry(rechargeGauge->m_x, rechargeGauge->m_y, rechargeGauge->m_w, rechargeGauge->m_h);
    state->setRechargeGaugeProgress(rechargeGauge->GetAmount(), rechargeGauge->GetTotalAmount());
}

void PopulateInventoryState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIItemWnd* const itemWnd = g_windowMgr.m_itemWnd;
    const bool visible = itemWnd && itemWnd->m_show != 0;
    state->setInventoryVisible(visible);
    if (!visible) {
        state->setInventoryGeometry(0, 0, 0, 0);
        state->setInventoryMini(false);
        state->setInventoryTab(0);
        state->setInventoryData(QVariantMap{});
        return;
    }

    state->setInventoryGeometry(itemWnd->m_x, itemWnd->m_y, itemWnd->m_w, itemWnd->m_h);
    state->setInventoryMini(itemWnd->IsMiniMode());

    UIItemWnd::DisplayData display{};
    QVariantMap data;
    if (itemWnd->GetDisplayDataForQt(&display)) {
        state->setInventoryTab(display.currentTab);
        data.insert(QStringLiteral("title"), ToQString(display.title));
        data.insert(QStringLiteral("currentItemCount"), display.currentItemCount);
        data.insert(QStringLiteral("maxItemCount"), display.maxItemCount);
        data.insert(QStringLiteral("viewOffset"), display.viewOffset);
        data.insert(QStringLiteral("maxViewOffset"), display.maxViewOffset);
        data.insert(QStringLiteral("scrollBarVisible"), display.scrollBarVisible);
        data.insert(QStringLiteral("scrollTrackX"), display.scrollTrackX);
        data.insert(QStringLiteral("scrollTrackY"), display.scrollTrackY);
        data.insert(QStringLiteral("scrollTrackWidth"), display.scrollTrackWidth);
        data.insert(QStringLiteral("scrollTrackHeight"), display.scrollTrackHeight);
        data.insert(QStringLiteral("scrollThumbX"), display.scrollThumbX);
        data.insert(QStringLiteral("scrollThumbY"), display.scrollThumbY);
        data.insert(QStringLiteral("scrollThumbWidth"), display.scrollThumbWidth);
        data.insert(QStringLiteral("scrollThumbHeight"), display.scrollThumbHeight);

        QVariantList systemButtons;
        systemButtons.reserve(itemWnd->GetQtSystemButtonCount());
        for (int index = 0; index < itemWnd->GetQtSystemButtonCount(); ++index) {
            UIItemWnd::QtButtonDisplay buttonDisplay{};
            if (!itemWnd->GetQtSystemButtonDisplayForQt(index, &buttonDisplay)) {
                continue;
            }

            QVariantMap button;
            button.insert(QStringLiteral("id"), buttonDisplay.id);
            button.insert(QStringLiteral("x"), buttonDisplay.x);
            button.insert(QStringLiteral("y"), buttonDisplay.y);
            button.insert(QStringLiteral("width"), buttonDisplay.width);
            button.insert(QStringLiteral("height"), buttonDisplay.height);
            button.insert(QStringLiteral("label"), ToQString(buttonDisplay.label));
            button.insert(QStringLiteral("visible"), buttonDisplay.visible);
            button.insert(QStringLiteral("active"), buttonDisplay.active);
            systemButtons.push_back(button);
        }
        data.insert(QStringLiteral("systemButtons"), systemButtons);

        QVariantList tabs;
        tabs.reserve(itemWnd->GetQtTabCount());
        for (int index = 0; index < itemWnd->GetQtTabCount(); ++index) {
            UIItemWnd::QtButtonDisplay tabDisplay{};
            if (!itemWnd->GetQtTabDisplayForQt(index, &tabDisplay)) {
                continue;
            }

            QVariantMap tab;
            tab.insert(QStringLiteral("id"), tabDisplay.id);
            tab.insert(QStringLiteral("x"), tabDisplay.x);
            tab.insert(QStringLiteral("y"), tabDisplay.y);
            tab.insert(QStringLiteral("width"), tabDisplay.width);
            tab.insert(QStringLiteral("height"), tabDisplay.height);
            tab.insert(QStringLiteral("label"), ToQString(tabDisplay.label));
            tab.insert(QStringLiteral("visible"), tabDisplay.visible);
            tab.insert(QStringLiteral("active"), tabDisplay.active);
            tabs.push_back(tab);
        }
        data.insert(QStringLiteral("tabs"), tabs);

        QVariantList itemSlots;
        itemSlots.reserve(static_cast<qsizetype>(display.displaySlots.size()));
        for (const UIItemWnd::DisplaySlot& slot : display.displaySlots) {
            QVariantMap entry;
            entry.insert(QStringLiteral("x"), slot.x);
            entry.insert(QStringLiteral("y"), slot.y);
            entry.insert(QStringLiteral("width"), slot.width);
            entry.insert(QStringLiteral("height"), slot.height);
            entry.insert(QStringLiteral("occupied"), slot.occupied);
            entry.insert(QStringLiteral("hovered"), slot.hovered);
            entry.insert(QStringLiteral("count"), slot.count);
            entry.insert(QStringLiteral("itemId"), static_cast<uint>(slot.itemId));
            entry.insert(QStringLiteral("label"), ToQString(slot.label));
            entry.insert(QStringLiteral("tooltip"), ToQString(slot.tooltip));
            itemSlots.push_back(entry);
        }

        data.insert(QStringLiteral("slots"), itemSlots);
    } else {
        state->setInventoryTab(0);
    }
    state->setInventoryData(data);
}

void PopulateEquipState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIEquipWnd* const equipWnd = g_windowMgr.m_equipWnd;
    const bool visible = equipWnd && equipWnd->m_show != 0;
    state->setEquipVisible(visible);
    if (!visible) {
        state->setEquipGeometry(0, 0, 0, 0);
        state->setEquipMini(false);
        state->setEquipData(QVariantMap{});
        return;
    }

    state->setEquipGeometry(equipWnd->m_x, equipWnd->m_y, equipWnd->m_w, equipWnd->m_h);
    state->setEquipMini(equipWnd->IsMiniMode());

    UIEquipWnd::DisplayData display{};
    QVariantMap data;
    if (equipWnd->GetDisplayDataForQt(&display)) {
        data.insert(QStringLiteral("title"), QStringLiteral("Equipment"));
        data.insert(
            QStringLiteral("previewRevision"),
            QString::number(static_cast<qulonglong>(equipWnd->GetQtPreviewRevision())));
        QVariantList systemButtons;
        systemButtons.reserve(equipWnd->GetQtSystemButtonCount());
        for (int index = 0; index < equipWnd->GetQtSystemButtonCount(); ++index) {
            UIEquipWnd::QtButtonDisplay buttonDisplay{};
            if (!equipWnd->GetQtSystemButtonDisplayForQt(index, &buttonDisplay)) {
                continue;
            }

            QVariantMap button;
            button.insert(QStringLiteral("id"), buttonDisplay.id);
            button.insert(QStringLiteral("x"), buttonDisplay.x);
            button.insert(QStringLiteral("y"), buttonDisplay.y);
            button.insert(QStringLiteral("width"), buttonDisplay.width);
            button.insert(QStringLiteral("height"), buttonDisplay.height);
            button.insert(QStringLiteral("label"), ToQString(buttonDisplay.label));
            button.insert(QStringLiteral("visible"), buttonDisplay.visible);
            systemButtons.push_back(button);
        }
        data.insert(QStringLiteral("systemButtons"), systemButtons);

        QVariantList equipSlots;
        equipSlots.reserve(static_cast<qsizetype>(display.displaySlots.size()));
        for (const UIEquipWnd::DisplaySlot& slot : display.displaySlots) {
            QVariantMap entry;
            entry.insert(QStringLiteral("x"), slot.x);
            entry.insert(QStringLiteral("y"), slot.y);
            entry.insert(QStringLiteral("width"), slot.width);
            entry.insert(QStringLiteral("height"), slot.height);
            entry.insert(QStringLiteral("occupied"), slot.occupied);
            entry.insert(QStringLiteral("hovered"), slot.hovered);
            entry.insert(QStringLiteral("leftColumn"), slot.leftColumn);
            entry.insert(QStringLiteral("itemId"), static_cast<uint>(slot.itemId));
            entry.insert(QStringLiteral("label"), ToQString(slot.label));
            equipSlots.push_back(entry);
        }
        data.insert(QStringLiteral("slots"), equipSlots);
    }
    state->setEquipData(data);
}

void PopulateSkillListState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UISkillListWnd* const skillWnd = g_windowMgr.m_skillListWnd;
    const bool visible = skillWnd && skillWnd->m_show != 0;
    state->setSkillListVisible(visible);
    if (!visible) {
        state->setSkillListGeometry(0, 0, 0, 0);
        state->setSkillListData(QVariantMap{});
        return;
    }

    state->setSkillListGeometry(skillWnd->m_x, skillWnd->m_y, skillWnd->m_w, skillWnd->m_h);

    UISkillListWnd::DisplayData display{};
    QVariantMap data;
    if (skillWnd->GetDisplayDataForQt(&display)) {
        data.insert(QStringLiteral("title"), QStringLiteral("Skill Tree"));
        QVariantList systemButtons;
        systemButtons.reserve(skillWnd->GetQtSystemButtonCount());
        for (int index = 0; index < skillWnd->GetQtSystemButtonCount(); ++index) {
            UISkillListWnd::QtButtonDisplay buttonDisplay{};
            if (!skillWnd->GetQtSystemButtonDisplayForQt(index, &buttonDisplay)) {
                continue;
            }

            QVariantMap button;
            button.insert(QStringLiteral("id"), buttonDisplay.id);
            button.insert(QStringLiteral("x"), buttonDisplay.x);
            button.insert(QStringLiteral("y"), buttonDisplay.y);
            button.insert(QStringLiteral("width"), buttonDisplay.width);
            button.insert(QStringLiteral("height"), buttonDisplay.height);
            button.insert(QStringLiteral("label"), ToQString(buttonDisplay.label));
            button.insert(QStringLiteral("visible"), buttonDisplay.visible);
            systemButtons.push_back(button);
        }
        data.insert(QStringLiteral("systemButtons"), systemButtons);

        data.insert(QStringLiteral("skillPointCount"), display.skillPointCount);
        data.insert(QStringLiteral("skillPointText"), QStringLiteral("Skill Point : %1").arg(display.skillPointCount));
        data.insert(QStringLiteral("upgradeLabel"), QStringLiteral("+"));
        data.insert(QStringLiteral("viewOffset"), display.viewOffset);
        data.insert(QStringLiteral("maxViewOffset"), display.maxViewOffset);
        data.insert(QStringLiteral("scrollBarVisible"), display.scrollBarVisible);
        data.insert(QStringLiteral("scrollTrackX"), display.scrollTrackX);
        data.insert(QStringLiteral("scrollTrackY"), display.scrollTrackY);
        data.insert(QStringLiteral("scrollTrackWidth"), display.scrollTrackWidth);
        data.insert(QStringLiteral("scrollTrackHeight"), display.scrollTrackHeight);
        data.insert(QStringLiteral("scrollThumbX"), display.scrollThumbX);
        data.insert(QStringLiteral("scrollThumbY"), display.scrollThumbY);
        data.insert(QStringLiteral("scrollThumbWidth"), display.scrollThumbWidth);
        data.insert(QStringLiteral("scrollThumbHeight"), display.scrollThumbHeight);

        QVariantList rows;
        rows.reserve(static_cast<qsizetype>(display.rows.size()));
        for (const UISkillListWnd::DisplayRow& row : display.rows) {
            QVariantMap entry;
            entry.insert(QStringLiteral("skillId"), row.skillId);
            entry.insert(QStringLiteral("x"), row.x);
            entry.insert(QStringLiteral("y"), row.y);
            entry.insert(QStringLiteral("width"), row.width);
            entry.insert(QStringLiteral("height"), row.height);
            entry.insert(QStringLiteral("iconVisible"), row.iconVisible);
            entry.insert(QStringLiteral("selected"), row.selected);
            entry.insert(QStringLiteral("hovered"), row.hovered);
            entry.insert(QStringLiteral("upgradeVisible"), row.upgradeVisible);
            entry.insert(QStringLiteral("upgradePressed"), row.upgradePressed);
            entry.insert(QStringLiteral("upgradeX"), row.upgradeX);
            entry.insert(QStringLiteral("upgradeY"), row.upgradeY);
            entry.insert(QStringLiteral("upgradeWidth"), row.upgradeWidth);
            entry.insert(QStringLiteral("upgradeHeight"), row.upgradeHeight);
            entry.insert(QStringLiteral("name"), ToQString(row.name));
            entry.insert(QStringLiteral("levelText"), ToQString(row.levelText));
            entry.insert(QStringLiteral("rightText"), ToQString(row.rightText));
            rows.push_back(entry);
        }
        data.insert(QStringLiteral("rows"), rows);

        QVariantList buttons;
        buttons.reserve(static_cast<qsizetype>(display.bottomButtons.size()));
        for (const UISkillListWnd::DisplayButton& button : display.bottomButtons) {
            QVariantMap entry;
            entry.insert(QStringLiteral("x"), button.x);
            entry.insert(QStringLiteral("y"), button.y);
            entry.insert(QStringLiteral("width"), button.width);
            entry.insert(QStringLiteral("height"), button.height);
            entry.insert(QStringLiteral("hovered"), button.hovered);
            entry.insert(QStringLiteral("pressed"), button.pressed);
            entry.insert(QStringLiteral("label"), ToQString(button.label));
            buttons.push_back(entry);
        }
        data.insert(QStringLiteral("bottomButtons"), buttons);
    }
    state->setSkillListData(data);
}

void PopulateOptionState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIOptionWnd* const optionWnd = g_windowMgr.m_optionWnd;
    const bool visible = optionWnd && optionWnd->m_show != 0;
    state->setOptionVisible(visible);
    if (!visible) {
        state->setOptionGeometry(0, 0, 0, 0);
        state->setOptionData(QVariantMap{});
        return;
    }

    state->setOptionGeometry(optionWnd->m_x, optionWnd->m_y, optionWnd->m_w, optionWnd->m_h);

    UIOptionWnd::DisplayData display{};
    QVariantMap data;
    if (optionWnd->GetDisplayDataForQt(&display)) {
        data.insert(QStringLiteral("title"), QStringLiteral("Options"));
        data.insert(QStringLiteral("collapsed"), display.collapsed);
        data.insert(QStringLiteral("activeTab"), display.activeTab);
        data.insert(QStringLiteral("contentX"), display.contentX);
        data.insert(QStringLiteral("contentY"), display.contentY);
        data.insert(QStringLiteral("contentWidth"), display.contentWidth);
        data.insert(QStringLiteral("contentHeight"), display.contentHeight);

        QVariantList systemButtons;
        systemButtons.reserve(static_cast<qsizetype>(display.systemButtons.size()));
        for (const UIOptionWnd::DisplayButton& buttonDisplay : display.systemButtons) {
            QVariantMap button;
            button.insert(QStringLiteral("x"), buttonDisplay.x);
            button.insert(QStringLiteral("y"), buttonDisplay.y);
            button.insert(QStringLiteral("width"), buttonDisplay.width);
            button.insert(QStringLiteral("height"), buttonDisplay.height);
            button.insert(QStringLiteral("visible"), buttonDisplay.visible);
            button.insert(QStringLiteral("label"), ToQString(buttonDisplay.label));
            systemButtons.push_back(button);
        }
        data.insert(QStringLiteral("systemButtons"), systemButtons);

        QVariantMap restartButton;
        restartButton.insert(QStringLiteral("x"), display.restartButton.x);
        restartButton.insert(QStringLiteral("y"), display.restartButton.y);
        restartButton.insert(QStringLiteral("width"), display.restartButton.width);
        restartButton.insert(QStringLiteral("height"), display.restartButton.height);
        restartButton.insert(QStringLiteral("visible"), display.restartButton.visible);
        restartButton.insert(QStringLiteral("label"), ToQString(display.restartButton.label));
        data.insert(QStringLiteral("restartButton"), restartButton);

        QVariantList tabs;
        tabs.reserve(static_cast<qsizetype>(display.tabs.size()));
        for (const UIOptionWnd::DisplayTab& tab : display.tabs) {
            QVariantMap entry;
            entry.insert(QStringLiteral("x"), tab.x);
            entry.insert(QStringLiteral("y"), tab.y);
            entry.insert(QStringLiteral("width"), tab.width);
            entry.insert(QStringLiteral("height"), tab.height);
            entry.insert(QStringLiteral("active"), tab.active);
            entry.insert(QStringLiteral("label"), ToQString(tab.label));
            tabs.push_back(entry);
        }
        data.insert(QStringLiteral("tabs"), tabs);

        QVariantList toggles;
        toggles.reserve(static_cast<qsizetype>(display.toggles.size()));
        for (const UIOptionWnd::DisplayToggle& toggle : display.toggles) {
            QVariantMap entry;
            entry.insert(QStringLiteral("x"), toggle.x);
            entry.insert(QStringLiteral("y"), toggle.y);
            entry.insert(QStringLiteral("width"), toggle.width);
            entry.insert(QStringLiteral("height"), toggle.height);
            entry.insert(QStringLiteral("checked"), toggle.checked);
            entry.insert(QStringLiteral("label"), ToQString(toggle.label));
            toggles.push_back(entry);
        }
        data.insert(QStringLiteral("toggles"), toggles);

        QVariantList sliders;
        sliders.reserve(static_cast<qsizetype>(display.sliders.size()));
        for (const UIOptionWnd::DisplaySlider& slider : display.sliders) {
            QVariantMap entry;
            entry.insert(QStringLiteral("x"), slider.x);
            entry.insert(QStringLiteral("y"), slider.y);
            entry.insert(QStringLiteral("width"), slider.width);
            entry.insert(QStringLiteral("height"), slider.height);
            entry.insert(QStringLiteral("value"), slider.value);
            entry.insert(QStringLiteral("label"), ToQString(slider.label));
            sliders.push_back(entry);
        }
        data.insert(QStringLiteral("sliders"), sliders);

        QVariantList graphicsRows;
        graphicsRows.reserve(static_cast<qsizetype>(display.graphicsRows.size()));
        for (const UIOptionWnd::DisplayGraphicsRow& row : display.graphicsRows) {
            QVariantMap entry;
            entry.insert(QStringLiteral("x"), row.x);
            entry.insert(QStringLiteral("y"), row.y);
            entry.insert(QStringLiteral("width"), row.width);
            entry.insert(QStringLiteral("height"), row.height);
            entry.insert(QStringLiteral("prevX"), row.prevX);
            entry.insert(QStringLiteral("prevY"), row.prevY);
            entry.insert(QStringLiteral("prevWidth"), row.prevWidth);
            entry.insert(QStringLiteral("prevHeight"), row.prevHeight);
            entry.insert(QStringLiteral("nextX"), row.nextX);
            entry.insert(QStringLiteral("nextY"), row.nextY);
            entry.insert(QStringLiteral("nextWidth"), row.nextWidth);
            entry.insert(QStringLiteral("nextHeight"), row.nextHeight);
            entry.insert(QStringLiteral("prevLabel"), ToQString(row.prevLabel));
            entry.insert(QStringLiteral("nextLabel"), ToQString(row.nextLabel));
            entry.insert(QStringLiteral("label"), ToQString(row.label));
            entry.insert(QStringLiteral("value"), ToQString(row.value));
            graphicsRows.push_back(entry);
        }
        data.insert(QStringLiteral("graphicsRows"), graphicsRows);
    }
    state->setOptionData(data);
}

void PopulateMinimapState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIRoMapWnd* const minimapWnd = g_windowMgr.m_roMapWnd;
    const bool visible = minimapWnd && minimapWnd->m_show != 0;
    state->setMinimapVisible(visible);
    if (!visible) {
        state->setMinimapGeometry(0, 0, 0, 0);
        state->setMinimapData(QVariantMap{});
        return;
    }

    state->setMinimapGeometry(minimapWnd->m_x, minimapWnd->m_y, minimapWnd->m_w, minimapWnd->m_h);

    UIRoMapWnd::DisplayData display{};
    QVariantMap data;
    if (minimapWnd->GetDisplayDataForQt(&display)) {
        const QString mapName = ToQString(display.mapName);
        data.insert(QStringLiteral("title"), mapName.isEmpty()
            ? QStringLiteral("Mini Map")
            : QStringLiteral("Mini Map - %1").arg(mapName));
        data.insert(QStringLiteral("closeLabel"), QStringLiteral("x"));
        data.insert(QStringLiteral("mapX"), display.mapX);
        data.insert(QStringLiteral("mapY"), display.mapY);
        data.insert(QStringLiteral("mapWidth"), display.mapWidth);
        data.insert(QStringLiteral("mapHeight"), display.mapHeight);
        data.insert(QStringLiteral("playerVisible"), display.playerVisible);
        data.insert(QStringLiteral("playerX"), display.playerX);
        data.insert(QStringLiteral("playerY"), display.playerY);
        data.insert(QStringLiteral("playerDirection"), display.playerDirection);
        data.insert(QStringLiteral("closeX"), display.closeX);
        data.insert(QStringLiteral("closeY"), display.closeY);
        data.insert(QStringLiteral("closeWidth"), display.closeWidth);
        data.insert(QStringLiteral("closeHeight"), display.closeHeight);
        data.insert(QStringLiteral("closePressed"), display.closePressed);
        data.insert(QStringLiteral("coordsX"), display.coordsX);
        data.insert(QStringLiteral("coordsY"), display.coordsY);
        data.insert(QStringLiteral("coordsWidth"), display.coordsWidth);
        data.insert(QStringLiteral("coordsHeight"), display.coordsHeight);
        data.insert(QStringLiteral("imageRevision"), display.imageRevision);
        data.insert(QStringLiteral("mapName"), mapName);
        data.insert(QStringLiteral("coordsText"), ToQString(display.coordsText));

        QVariantList markers;
        markers.reserve(static_cast<qsizetype>(display.markers.size()));
        for (const UIRoMapWnd::DisplayMarker& marker : display.markers) {
            QVariantMap entry;
            entry.insert(QStringLiteral("x"), marker.x);
            entry.insert(QStringLiteral("y"), marker.y);
            entry.insert(QStringLiteral("radius"), marker.radius);
            entry.insert(QStringLiteral("color"), QStringLiteral("#%1").arg(marker.color & 0x00FFFFFFu, 6, 16, QLatin1Char('0')));
            markers.push_back(entry);
        }
        data.insert(QStringLiteral("markers"), markers);
    }
    state->setMinimapData(data);
}

bool IsMonsterLikeHoverActor(const CGameActor* actor)
{
    if (!actor) {
        return false;
    }

    const int job = actor->m_job;
    return job >= 1000 && (job < 6001 || job > 6047);
}

bool ShouldUseServerNameForHoverActor(const CGameActor* actor)
{
    if (!actor) {
        return false;
    }

    if (actor->m_isPc) {
        return true;
    }

    if (IsMonsterLikeHoverActor(actor)) {
        return true;
    }

    return actor->m_objectType == 6;
}

void SendActorNameRequest(CGameMode& mode, u32 gid)
{
    if (gid == 0 || gid == g_session.m_gid) {
        return;
    }

    const u32 now = GetTickCount();
    const auto timerIt = mode.m_actorNameByGIDReqTimer.find(gid);
    if (timerIt != mode.m_actorNameByGIDReqTimer.end() && now - timerIt->second < kHoverNameRequestCooldownMs) {
        return;
    }

    PACKET_CZ_REQNAME2 packet{};
    packet.PacketType = PacketProfile::ActiveMapServerSend::kGetCharNameRequest;
    packet.GID = gid;
    if (CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), static_cast<int>(sizeof(packet)))) {
        mode.m_actorNameByGIDReqTimer[gid] = now;
    }
}

std::string ResolveActorLabel(const CGameMode& mode, CGameActor* actor)
{
    if (!actor) {
        return std::string();
    }

    if (actor->m_gid == g_session.m_gid) {
        const char* playerName = g_session.GetPlayerName();
        if (playerName && *playerName) {
            return playerName;
        }
    }

    const auto cachedNameIt = mode.m_actorNameListByGID.find(actor->m_gid);
    if (cachedNameIt != mode.m_actorNameListByGID.end() && !cachedNameIt->second.name.empty()) {
        return cachedNameIt->second.name;
    }

    if (ShouldUseServerNameForHoverActor(actor)) {
        SendActorNameRequest(const_cast<CGameMode&>(mode), actor->m_gid);
        if (actor->m_isPc) {
            return "Player";
        }
        if (IsMonsterLikeHoverActor(actor)) {
            return "Monster";
        }
        return "NPC";
    }

    const char* jobName = g_session.GetJobName(actor->m_job);
    if (jobName && *jobName) {
        return jobName;
    }

    return "Entity";
}

std::string ResolveGroundItemLabel(const CItem* item)
{
    return item ? shopui::BuildGroundItemHoverText(item->m_itemName,
                      item->m_itemId,
                      item->m_identified != 0,
                      static_cast<unsigned int>(item->m_amount))
                : std::string();
}

QString ResolveHoverBackground(const CGameActor* actor)
{
    if (!actor) {
        return QStringLiteral("#c05a1620");
    }

    if (IsMonsterLikeHoverActor(actor)) {
        return QStringLiteral("#c0be185d");
    }

    if (actor->m_isPc) {
        return QStringLiteral("#c05c2a86");
    }

    if (actor->m_objectType == 6) {
        return QStringLiteral("#c02d63b6");
    }

    return QStringLiteral("#c05a1620");
}

QString ResolveHoverForeground(const CGameActor* actor)
{
    if (!actor) {
        return QStringLiteral("#ffffff");
    }

    u32 actorId = actor->m_gid;
    if (actorId != 0 && (actorId == g_session.m_gid || actorId == g_session.m_aid)) {
        actorId = g_session.m_aid != 0 ? g_session.m_aid : actorId;
    }
    return IsNameYellow(actorId) ? QStringLiteral("#ffff00") : QStringLiteral("#ffffff");
}

QVariantMap MakeAnchor(const QString& text,
    int screenX,
    int screenY,
    const QString& background,
    const QString& foreground = QStringLiteral("#ffffff"),
    bool showBubble = true,
    int fontPixelSize = 14,
    bool bold = true)
{
    QVariantMap anchor;
    anchor.insert(QStringLiteral("text"), text);
    anchor.insert(QStringLiteral("x"), screenX + 12);
    anchor.insert(QStringLiteral("y"), screenY - 26);
    anchor.insert(QStringLiteral("background"), background);
    anchor.insert(QStringLiteral("foreground"), foreground);
    anchor.insert(QStringLiteral("showBubble"), showBubble);
    anchor.insert(QStringLiteral("fontPixelSize"), fontPixelSize);
    anchor.insert(QStringLiteral("bold"), bold);
    return anchor;
}

QVariantMap MakeCenteredAnchor(const QString& text,
    int centerX,
    int topY,
    const QString& background,
    const QString& foreground = QStringLiteral("#ffffff"),
    bool showBubble = true,
    int fontPixelSize = 14,
    bool bold = true)
{
    QFont font;
    font.setPixelSize(fontPixelSize);
    font.setBold(bold);
    const QFontMetrics metrics(font);
    const int textWidth = (std::max)(1, metrics.horizontalAdvance(text));
    const int textHeight = (std::max)(1, metrics.height());
    const int bubbleWidth = textWidth + (showBubble ? 12 : 0);
    const int bubbleHeight = textHeight + (showBubble ? 8 : 0);

    QVariantMap anchor;
    anchor.insert(QStringLiteral("text"), text);
    anchor.insert(QStringLiteral("x"), centerX - bubbleWidth / 2);
    anchor.insert(QStringLiteral("y"), topY);
    anchor.insert(QStringLiteral("background"), background);
    anchor.insert(QStringLiteral("foreground"), foreground);
    anchor.insert(QStringLiteral("showBubble"), showBubble);
    anchor.insert(QStringLiteral("fontPixelSize"), fontPixelSize);
    anchor.insert(QStringLiteral("bold"), bold);
    return anchor;
}

QVariantMap MakeUiItemAnchor(const shopui::ItemHoverInfo& hoverInfo)
{
    const int centerX = hoverInfo.anchorRect.left + ((hoverInfo.anchorRect.right - hoverInfo.anchorRect.left) / 2);
    return MakeCenteredAnchor(ToQString(hoverInfo.text),
        centerX,
        hoverInfo.anchorRect.top - 26,
        QStringLiteral("#c0087f5b"));
}

bool TryAppendHoveredUiItemAnchor(QVariantList* anchors)
{
    if (!anchors) {
        return false;
    }

    shopui::ItemHoverInfo hoverInfo{};
    if (g_windowMgr.m_itemWnd && g_windowMgr.m_itemWnd->GetHoveredItemForQt(&hoverInfo) && hoverInfo.IsValid()) {
        anchors->push_back(MakeUiItemAnchor(hoverInfo));
        return true;
    }

    hoverInfo = shopui::ItemHoverInfo{};
    if (g_windowMgr.m_equipWnd && g_windowMgr.m_equipWnd->GetHoveredItemForQt(&hoverInfo) && hoverInfo.IsValid()) {
        anchors->push_back(MakeUiItemAnchor(hoverInfo));
        return true;
    }

    hoverInfo = shopui::ItemHoverInfo{};
    if (g_windowMgr.m_shortCutWnd && g_windowMgr.m_shortCutWnd->GetHoveredItemForQt(&hoverInfo) && hoverInfo.IsValid()) {
        anchors->push_back(MakeUiItemAnchor(hoverInfo));
        return true;
    }

    return false;
}

} // namespace

QtUiStateAdapter::QtUiStateAdapter()
    : m_state(new QtUiState())
{
    m_state->setLastInput(QStringLiteral("No routed input yet."));
}

QtUiStateAdapter::~QtUiStateAdapter()
{
    delete m_state;
    m_state = nullptr;
}

QObject* QtUiStateAdapter::stateObject() const
{
    return m_state;
}

void QtUiStateAdapter::setLastInput(const QString& value)
{
    if (m_state) {
        m_state->setLastInput(value.isEmpty() ? QStringLiteral("No routed input yet.") : value);
    }
}

bool QtUiStateAdapter::syncMenu(RenderBackendType activeBackend,
    RenderBackendType nativeOverlayBackend)
{
    if (!m_state) {
        return false;
    }

    const QString backendName = BackendToQString(activeBackend);
    const QString modeName = BuildMenuModeText();
    const QString renderPath = BuildRenderPathText(nativeOverlayBackend);
    const QString loginStatus = ToQString(g_windowMgr.GetLoginStatus());
    const QString chatPreview = BuildChatPreviewText();
    const QString lastInput = m_state->lastInput();

    m_state->setBackendName(backendName);
    m_state->setModeName(modeName);
    m_state->setRenderPath(renderPath);
    m_state->setArchitectureNote(BuildArchitectureNote(nativeOverlayBackend));
    m_state->setLoginStatus(loginStatus);
    m_state->setChatPreview(chatPreview);
    m_state->setDebugOverlayData(BuildDebugOverlayData(
        backendName,
        modeName,
        renderPath,
        loginStatus,
        chatPreview,
        lastInput));
    PopulateLoginPanelState(m_state);
    PopulateCharSelectState(m_state);
    PopulateMakeCharState(m_state);
    PopulateServerSelectState(m_state);
    PopulateLoadingState(m_state);
    m_state->setNpcMenuVisible(false);
    m_state->setNpcMenuGeometry(0, 0, 0, 0);
    m_state->setNpcMenuSelection(-1);
    m_state->setNpcMenuHoverIndex(-1);
    m_state->setNpcMenuButtons(false, false);
    m_state->setNpcMenuOptions(QVariantList{});
    m_state->setSayDialogVisible(false);
    m_state->setSayDialogGeometry(0, 0, 0, 0);
    m_state->setSayDialogText(QString());
    m_state->setSayDialogAction(false, QString(), false, false);
    m_state->setNpcInputVisible(false);
    m_state->setNpcInputGeometry(0, 0, 0, 0);
    m_state->setNpcInputText(QString(), QString());
    m_state->setNpcInputButtons(false, false);
    m_state->setChooseMenuVisible(false);
    m_state->setChooseMenuGeometry(0, 0, 0, 0);
    m_state->setChooseMenuSelectedIndex(-1);
    m_state->setChooseMenuPressedIndex(-1);
    m_state->setChooseMenuOptions(QVariantList{});
    m_state->setItemShopVisible(false);
    m_state->setItemShopGeometry(0, 0, 0, 0);
    m_state->setItemShopTitle(QString());
    m_state->setItemShopRows(QVariantList{});
    m_state->setItemPurchaseVisible(false);
    m_state->setItemPurchaseGeometry(0, 0, 0, 0);
    m_state->setItemPurchaseTotal(0);
    m_state->setItemPurchaseRows(QVariantList{});
    m_state->setItemPurchaseButtons(QVariantList{});
    m_state->setItemSellVisible(false);
    m_state->setItemSellGeometry(0, 0, 0, 0);
    m_state->setItemSellTotal(0);
    m_state->setItemSellRows(QVariantList{});
    m_state->setItemSellButtons(QVariantList{});
    m_state->setShortCutVisible(false);
    m_state->setShortCutGeometry(0, 0, 0, 0);
    m_state->setShortCutPage(0);
    m_state->setShortCutHoverSlot(-1);
    m_state->setShortCutSlots(QVariantList{});
    m_state->setBasicInfoVisible(false);
    m_state->setBasicInfoGeometry(0, 0, 0, 0);
    m_state->setBasicInfoMini(false);
    m_state->setBasicInfoData(QVariantMap{});
    m_state->setShopChoiceVisible(false);
    m_state->setShopChoiceGeometry(0, 0, 0, 0);
    m_state->setShopChoiceText(QString(), QString());
    m_state->setShopChoiceButtons(QVariantList{});
    m_state->setNotifications(QVariantList{});
    m_state->setAnchors(QVariantList{});
    return true;
}

bool QtUiStateAdapter::syncGameplay(CGameMode& mode,
    RenderBackendType activeBackend,
    RenderBackendType nativeOverlayBackend,
    int mouseX,
    int mouseY)
{
    if (!m_state) {
        return false;
    }

    const QString backendName = BackendToQString(activeBackend);
    const QString modeName = QStringLiteral("Gameplay");
    const QString renderPath = BuildRenderPathText(nativeOverlayBackend);
    const QString loginStatus = ToQString(g_windowMgr.GetLoginStatus());
    const QString chatPreview = BuildChatPreviewText();
    const QString lastInput = m_state->lastInput();

    m_state->setBackendName(backendName);
    m_state->setModeName(modeName);
    m_state->setRenderPath(renderPath);
    m_state->setArchitectureNote(BuildArchitectureNote(nativeOverlayBackend));
    m_state->setLoginStatus(loginStatus);
    m_state->setChatPreview(chatPreview);
    m_state->setDebugOverlayData(BuildDebugOverlayData(
        backendName,
        modeName,
        renderPath,
        loginStatus,
        chatPreview,
        lastInput));
    PopulateLoginPanelState(m_state);
    PopulateCharSelectState(m_state);
    PopulateMakeCharState(m_state);
    PopulateServerSelectState(m_state);
    PopulateLoadingState(m_state);
    PopulateNpcMenuState(m_state);
    PopulateSayDialogState(m_state);
    PopulateNpcInputState(m_state);
    PopulateChooseMenuState(m_state);
    PopulateItemShopState(m_state);
    PopulateItemPurchaseState(m_state);
    PopulateItemSellState(m_state);
    PopulateShortCutState(m_state);
    PopulateBasicInfoState(m_state);
    PopulateStatusState(m_state);
    PopulateChatWindowState(m_state);
    PopulateRechargeGaugeState(m_state);
    PopulateInventoryState(m_state);
    PopulateEquipState(m_state);
    PopulateSkillListState(m_state);
    PopulateOptionState(m_state);
    PopulateMinimapState(m_state);
    PopulateShopChoiceState(m_state);
    PopulateNotificationState(m_state);

    QVariantList anchors;
    if (mode.m_world && mode.m_view) {
        const matrix& viewMatrix = mode.m_view->GetViewMatrix();
        const float cameraLongitude = mode.m_view->GetCameraLongitude();

        const bool hasUiItemHover = TryAppendHoveredUiItemAnchor(&anchors);
        const bool blocksWorldHover = g_windowMgr.HasWindowAtPoint(mouseX, mouseY);

        int labelX = 0;
        int labelY = 0;
        CGameActor* hoveredActor = nullptr;
        if (!hasUiItemHover && !blocksWorldHover && mode.m_world->FindHoveredActorScreen(viewMatrix,
                cameraLongitude,
                mouseX,
                mouseY,
                &hoveredActor,
                &labelX,
                &labelY)) {
            if (!hoveredActor || hoveredActor->m_gid != mode.m_lastLockOnMonGid) {
                int anchorX = labelX;
                int anchorY = labelY + kQtActorLabelVerticalOffset;
                if (hoveredActor == mode.m_world->m_player || hoveredActor->m_gid == g_session.m_gid) {
                    mode.m_world->GetPlayerScreenLabel(viewMatrix, cameraLongitude, &anchorX, &anchorY);
                    anchorY += kQtActorLabelVerticalOffset;
                } else {
                    mode.m_world->GetActorScreenMarker(viewMatrix, cameraLongitude, hoveredActor->m_gid, &anchorX, nullptr, &anchorY);
                    anchorY += kQtActorLabelVerticalOffset;
                }
                anchors.push_back(MakeCenteredAnchor(ToQString(ResolveActorLabel(mode, hoveredActor)),
                    anchorX,
                    anchorY,
                    ResolveHoverBackground(hoveredActor),
                    ResolveHoverForeground(hoveredActor)));
            }
        } else if (!hasUiItemHover && !blocksWorldHover) {
            CItem* hoveredItem = nullptr;
            if (mode.m_world->FindHoveredGroundItemScreen(viewMatrix,
                    mouseX,
                    mouseY,
                    &hoveredItem,
                    &labelX,
                    &labelY)) {
                anchors.push_back(MakeAnchor(ToQString(ResolveGroundItemLabel(hoveredItem)),
                    labelX,
                    labelY,
                    QStringLiteral("#c0087f5b")));
            }
        }

        if (mode.m_lastLockOnMonGid != 0) {
            const auto actorIt = mode.m_runtimeActors.find(mode.m_lastLockOnMonGid);
            if (actorIt != mode.m_runtimeActors.end() && actorIt->second && actorIt->second->m_isVisible) {
                if (mode.m_world->GetActorScreenMarker(viewMatrix,
                        cameraLongitude,
                        mode.m_lastLockOnMonGid,
                        &labelX,
                        nullptr,
                        &labelY)) {
                    const QString lockLabel = ToQString(ResolveActorLabel(mode, actorIt->second));
                    if (!lockLabel.isEmpty()) {
                        anchors.push_back(MakeCenteredAnchor(QStringLiteral("▼"),
                            labelX - 6,
                            labelY - 24,
                            QStringLiteral("transparent"),
                            ToCssColor(255, 226, 120),
                            false,
                            20,
                            true));
                        anchors.push_back(MakeCenteredAnchor(lockLabel,
                            labelX,
                            labelY + kQtActorLabelVerticalOffset,
                            ResolveHoverBackground(actorIt->second),
                            ResolveHoverForeground(actorIt->second)));
                    }
                }
            }
        }
    }

    m_state->setAnchors(anchors);
    return true;
}
