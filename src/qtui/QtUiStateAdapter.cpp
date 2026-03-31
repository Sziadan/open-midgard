#include "QtUiStateAdapter.h"

#include "QtUiState.h"

#include "network/Connection.h"
#include "network/Packet.h"
#include "core/ClientInfoLocale.h"
#include "gamemode/GameMode.h"
#include "gamemode/View.h"
#include "render3d/RenderBackend.h"
#include "render3d/RenderDevice.h"
#include "session/Session.h"
#include "ui/UILoginWnd.h"
#include "ui/UILoadingWnd.h"
#include "ui/UIMakeCharWnd.h"
#include "ui/UIChooseWnd.h"
#include "ui/UIChooseSellBuyWnd.h"
#include "ui/UIBasicInfoWnd.h"
#include "ui/UIItemPurchaseWnd.h"
#include "ui/UIItemSellWnd.h"
#include "ui/UIItemShopWnd.h"
#include "ui/UINewChatWnd.h"
#include "ui/UINpcMenuWnd.h"
#include "ui/UINpcInputWnd.h"
#include "ui/UINotifyLevelUpWnd.h"
#include "ui/UIRechargeGage.h"
#include "ui/UISelectCharWnd.h"
#include "ui/UISelectServerWnd.h"
#include "ui/UISayDialogWnd.h"
#include "ui/UIShortCutWnd.h"
#include "ui/UIStatusWnd.h"
#include "ui/UIShopCommon.h"
#include "ui/UIWindowMgr.h"
#include "world/GameActor.h"
#include "world/World.h"

#include <QChar>
#include <QStringList>
#include <QVariantMap>

#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr DWORD kHoverNameRequestCooldownMs = 1000;

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

QString StripNpcColorCodes(const std::string& value)
{
    std::string stripped;
    stripped.reserve(value.size());
    for (size_t index = 0; index < value.size(); ++index) {
        if (IsNpcColorCodeAt(value, index)) {
            index += 6;
            continue;
        }
        stripped.push_back(value[index]);
    }
    return ToQString(stripped);
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
        state->setServerEntries(QVariantList{});
        return;
    }

    state->setServerPanelGeometry(serverWnd->m_x, serverWnd->m_y, serverWnd->m_w, serverWnd->m_h);
    state->setServerSelectedIndex(GetSelectedClientInfoIndex());
    state->setServerHoverIndex(serverWnd->GetHoverIndex());

    QVariantList entries;
    const std::vector<ClientInfoConnection>& connections = GetClientInfoConnections();
    entries.reserve(static_cast<qsizetype>(connections.size()));
    for (const ClientInfoConnection& info : connections) {
        QVariantMap entry;
        entry.insert(QStringLiteral("label"), ToQString((!info.display.empty() ? info.display : info.address).c_str()));
        entry.insert(QStringLiteral("detail"), ToQString((!info.desc.empty() ? info.desc : info.port).c_str()));
        entries.push_back(entry);
    }
    state->setServerEntries(entries);
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
        return;
    }

    state->setLoginPanelGeometry(loginWnd->m_x, loginWnd->m_y, loginWnd->m_w, loginWnd->m_h);
    const QString userId = ToQString(loginWnd->GetLoginText());
    const QString passwordMask(loginWnd->GetPasswordLength(), QLatin1Char('*'));
    state->setLoginPanelData(
        userId,
        passwordMask,
        loginWnd->IsSaveAccountChecked(),
        loginWnd->IsPasswordFocused());
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
        return;
    }

    state->setCharSelectPanelGeometry(
        selectCharWnd->m_x,
        selectCharWnd->m_y,
        selectCharWnd->m_w,
        selectCharWnd->m_h);
    state->setCharSelectPageState(
        selectCharWnd->GetCurrentPage(),
        selectCharWnd->GetCurrentPageCount());

    QVariantList slots;
    for (int visibleIndex = 0; visibleIndex < 3; ++visibleIndex) {
        UISelectCharWnd::VisibleSlotDisplay slotInfo{};
        if (!selectCharWnd->GetVisibleSlotDisplay(visibleIndex, &slotInfo)) {
            continue;
        }

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
        slots.push_back(slot);
    }
    state->setCharSelectSlots(slots);

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
    }
    state->setCharSelectSelectedDetails(details);
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
        return;
    }

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
        return;
    }

    state->setMakeCharData(QString(), false, QVariantList{}, 0, 0);
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
        state->setShopChoiceButtons(QVariantList{});
        return;
    }

    state->setShopChoiceGeometry(shopWnd->m_x, shopWnd->m_y, shopWnd->m_w, shopWnd->m_h);

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
        options.push_back(StripNpcColorCodes(option));
    }
    state->setNpcMenuOptions(options);
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
        return;
    }

    state->setSayDialogGeometry(dialogWnd->m_x, dialogWnd->m_y, dialogWnd->m_w, dialogWnd->m_h);
    state->setSayDialogText(StripNpcColorCodes(dialogWnd->GetDisplayText()));
    state->setSayDialogAction(
        dialogWnd->HasActionButton(),
        dialogWnd->IsNextAction() ? QStringLiteral("Next") : QStringLiteral("Close"),
        dialogWnd->IsHoveringAction(),
        dialogWnd->IsPressingAction());
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
        return;
    }

    state->setNpcInputGeometry(inputWnd->m_x, inputWnd->m_y, inputWnd->m_w, inputWnd->m_h);
    state->setNpcInputText(
        inputWnd->GetInputMode() == UINpcInputWnd::InputMode::Number
            ? QStringLiteral("Enter a number")
            : QStringLiteral("Enter text"),
        ToQString(inputWnd->GetInputText()));
    state->setNpcInputButtons(inputWnd->IsOkPressed(), inputWnd->IsCancelPressed());
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
        return;
    }

    state->setChooseMenuGeometry(chooseWnd->m_x, chooseWnd->m_y, chooseWnd->m_w, chooseWnd->m_h);
    state->setChooseMenuSelectedIndex(chooseWnd->GetSelectedIndex());
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
        state->setItemShopRows(QVariantList{});
        return;
    }

    state->setItemShopGeometry(shopWnd->m_x, shopWnd->m_y, shopWnd->m_w, shopWnd->m_h);
    state->setItemShopTitle(g_session.m_shopMode == NpcShopMode::Sell
            ? QStringLiteral("Sellable Items")
            : QStringLiteral("Shop Items"));

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
        state->setItemPurchaseRows(QVariantList{});
        state->setItemPurchaseButtons(QVariantList{});
        return;
    }

    state->setItemPurchaseGeometry(purchaseWnd->m_x, purchaseWnd->m_y, purchaseWnd->m_w, purchaseWnd->m_h);
    state->setItemPurchaseTotal(g_session.m_shopDealTotal);

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
        state->setItemSellRows(QVariantList{});
        state->setItemSellButtons(QVariantList{});
        return;
    }

    state->setItemSellGeometry(sellWnd->m_x, sellWnd->m_y, sellWnd->m_w, sellWnd->m_h);
    state->setItemSellTotal(g_session.m_shopDealTotal);

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

    QVariantList slots;
    slots.reserve(kShortcutSlotsPerPage);
    for (int slotIndex = 0; slotIndex < kShortcutSlotsPerPage; ++slotIndex) {
        QVariantMap slotEntry;
        slotEntry.insert(QStringLiteral("index"), slotIndex);
        slotEntry.insert(QStringLiteral("hover"), shortCutWnd->GetHoverSlot() == slotIndex);

        const SHORTCUT_SLOT* slot = g_session.GetShortcutSlotByVisibleIndex(slotIndex);
        if (slot && slot->id != 0) {
            slotEntry.insert(QStringLiteral("occupied"), true);
            slotEntry.insert(QStringLiteral("isSkill"), slot->isSkill != 0);
            slotEntry.insert(QStringLiteral("label"), ResolveShortcutSlotLabel(slot));
            slotEntry.insert(QStringLiteral("count"), static_cast<int>(slot->count));
        } else {
            slotEntry.insert(QStringLiteral("occupied"), false);
            slotEntry.insert(QStringLiteral("isSkill"), false);
            slotEntry.insert(QStringLiteral("label"), QString());
            slotEntry.insert(QStringLiteral("count"), 0);
        }
        slots.push_back(slotEntry);
    }
    state->setShortCutSlots(slots);
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
        data.insert(QStringLiteral("name"), ToQString(display.name));
        data.insert(QStringLiteral("jobName"), ToQString(display.jobName));
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
            row.insert(QStringLiteral("canIncrease"),
                !statusWnd->IsMiniMode()
                && statusWnd->GetPageForQt() == 0
                && display.statCosts[index] > 0
                && display.baseStats[index] < 99
                && display.statCosts[index] <= display.statusPoint);
            stats.push_back(row);
        }

        data.insert(QStringLiteral("stats"), stats);
        data.insert(QStringLiteral("attackText"), FormatCompositeStatusText(display.attack, display.refineAttack));
        data.insert(QStringLiteral("matkText"), FormatMatkStatusText(display.matkMin, display.matkMax));
        data.insert(QStringLiteral("hit"), display.hit);
        data.insert(QStringLiteral("critical"), display.critical);
        data.insert(QStringLiteral("statusPoint"), display.statusPoint);
        data.insert(QStringLiteral("itemDefText"), FormatCompositeStatusText(display.itemDef, display.plusDef));
        data.insert(QStringLiteral("itemMdefText"), FormatCompositeStatusText(display.itemMdef, display.plusMdef));
        data.insert(QStringLiteral("fleeText"), FormatCompositeStatusText(display.flee, display.plusFlee));
        data.insert(QStringLiteral("aspdText"), FormatCompositeStatusText(display.aspd, display.plusAspd));
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
    if (!item) {
        return std::string();
    }
    std::string itemName = item->m_itemName;
    if (itemName.empty() && item->m_itemId != 0) {
        itemName = g_ttemmgr.GetDisplayName(item->m_itemId, item->m_identified != 0);
    }
    if (itemName.empty()) {
        itemName = "Item";
    }

    char amountText[64]{};
    std::snprintf(amountText, sizeof(amountText), "%s: %u ea", itemName.c_str(), static_cast<unsigned int>(item->m_amount));
    return amountText;
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

    m_state->setBackendName(BackendToQString(activeBackend));
    m_state->setModeName(BuildMenuModeText());
    m_state->setRenderPath(BuildRenderPathText(nativeOverlayBackend));
    m_state->setArchitectureNote(BuildArchitectureNote(nativeOverlayBackend));
    m_state->setLoginStatus(ToQString(g_windowMgr.GetLoginStatus()));
    m_state->setChatPreview(BuildChatPreviewText());
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

    m_state->setBackendName(BackendToQString(activeBackend));
    m_state->setModeName(QStringLiteral("Gameplay"));
    m_state->setRenderPath(BuildRenderPathText(nativeOverlayBackend));
    m_state->setArchitectureNote(BuildArchitectureNote(nativeOverlayBackend));
    m_state->setLoginStatus(ToQString(g_windowMgr.GetLoginStatus()));
    m_state->setChatPreview(BuildChatPreviewText());
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
    PopulateShopChoiceState(m_state);
    PopulateNotificationState(m_state);

    QVariantList anchors;
    if (mode.m_world && mode.m_view) {
        const matrix& viewMatrix = mode.m_view->GetViewMatrix();
        const float cameraLongitude = mode.m_view->GetCameraLongitude();

        int labelX = 0;
        int labelY = 0;
        if (mode.m_world->GetPlayerScreenLabel(viewMatrix, cameraLongitude, &labelX, &labelY)) {
            QString playerName = ToQString(g_session.GetPlayerName());
            if (playerName.isEmpty()) {
                playerName = QStringLiteral("Player");
            }
            anchors.push_back(MakeAnchor(playerName, labelX, labelY, QStringLiteral("#c0813cf2")));
        }

        CGameActor* hoveredActor = nullptr;
        if (mode.m_world->FindHoveredActorScreen(viewMatrix,
                cameraLongitude,
                mouseX,
                mouseY,
                &hoveredActor,
                &labelX,
                &labelY)) {
            if (!hoveredActor || hoveredActor->m_gid != mode.m_lastLockOnMonGid) {
                anchors.push_back(MakeAnchor(ToQString(ResolveActorLabel(mode, hoveredActor)),
                    labelX,
                    labelY,
                    QStringLiteral("#c0be185d"),
                    ResolveHoverForeground(hoveredActor)));
            }
        } else {
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
                        anchors.push_back(MakeAnchor(QStringLiteral("▼"),
                            labelX - 6,
                            labelY - 24,
                            QStringLiteral("transparent"),
                            ToCssColor(255, 226, 120),
                            false,
                            20,
                            true));
                        anchors.push_back(MakeAnchor(lockLabel,
                            labelX,
                            labelY,
                            QStringLiteral("#c05a1620"),
                            ResolveHoverForeground(actorIt->second)));
                    }
                }
            }
        }
    }

    m_state->setAnchors(anchors);
    return true;
}
