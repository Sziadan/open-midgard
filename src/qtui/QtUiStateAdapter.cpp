#include "ui/StatusLayout.h"
#include "ui/UIChatRoomWnd.h"
#include "ui/UIChatRoomSettingsWnd.h"
#include "ui/UIWhisperWnd.h"
#include "ui/UIAutoMessageWnd.h"
#include "ui/UIFriendOptionWnd.h"
#include "ui/UICommandListWnd.h"
#include "ui/UIPetInfoWnd.h"
#include "ui/UICompanionStatusWnd.h"
#include "ui/UIInstanceWnd.h"
#include "ui/UIQuestWnd.h"
#include <ctime>
#include "ui/UICompanionSkillListWnd.h"
#include "ui/UIPetEggListWnd.h"
#include "ui/UIPetTamingDeceiveWnd.h"
#include "ui/ChatScrollBarUi.h"
#include "ui/UITipOfTheDayWnd.h"
#include "ui/UITransBalloonText.h"
#include "ui/ChatScrollBarUi.h"
#include "QtUiStateAdapter.h"

#include "QtUiState.h"
#include "QtCommerce.h"
#include "GuildEmblemUpload.h"
#include "GuildEmblemPicker.h"

#include "QtUiStatusIconCatalog.h"
#include "world/StatusStateIconMap.h"

#include "network/Connection.h"
#include "DebugLog.h"
#include "network/MapSendProfile.h"
#include "network/Packet.h"
#include "core/ClientInfoLocale.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "gamemode/PacketPadding.h"
#include "gamemode/View.h"
#include "main/WinMain.h"
#include "render3d/RenderBackend.h"
#include "render3d/RenderDevice.h"
#include "session/Session.h"
#include "skill/Skill.h"
#include "ui/UiScale.h"
#include "ui/UILoginWnd.h"
#include "ui/UILoadingWnd.h"
#include "ui/UIMakeCharWnd.h"
#include "ui/UIChooseWnd.h"
#include "ui/UIJoinPartyAcceptWnd.h"
#include "ui/UIChooseSellBuyWnd.h"
#include "ui/UIBasicInfoWnd.h"
#include "ui/UIEquipWnd.h"
#include "ui/UIItemCollectionWnd.h"
#include "ui/UIItemCompositionWnd.h"
#include "ui/UIItemIdentifyWnd.h"
#include "ui/UISkillSelectWnd.h"
#include "ui/UIItemInfoWnd.h"
#include "ui/UINotifyItemObtainWnd.h"
#include "ui/UIItemWnd.h"
#include "ui/UIMessengerGroupWnd.h"
#include "ui/UIPartyOptionWnd.h"
#include "ui/UIStorageWnd.h"
#include "ui/UIItemPurchaseWnd.h"
#include "ui/UIItemSellWnd.h"
#include "ui/UIItemShopWnd.h"
#include "ui/UISelectCartWnd.h"
#include "ui/UIMinimapWnd.h"
#include "ui/UINewChatWnd.h"
#include "ui/UINpcMenuWnd.h"
#include "ui/UIPlayerContextMenuWnd.h"
#include "ui/UINpcInputWnd.h"
#include "ui/UINotifyLevelUpWnd.h"
#include "ui/UIOptionWnd.h"
#include "ui/UIRechargeGage.h"
#include "ui/UISelectCharWnd.h"
#include "ui/UISelectServerWnd.h"
#include "ui/UISayDialogWnd.h"
#include "ui/UISkillDescribeWnd.h"
#include "ui/UISkillListWnd.h"
#include "ui/UIShortCutWnd.h"
#include "ui/UIStatusWnd.h"
#include "ui/UIShopCommon.h"
#include "ui/UIWaitWnd.h"
#include "ui/UIWindowMgr.h"
#include "ui/UIGuildWnd.h"
#include "world/GameActor.h"
#include "world/World.h"

#include <QChar>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QBuffer>
#include <QStringList>
#include <QVariantMap>
#include <QFile>
#include <QImageReader>
#include <QCoreApplication>
#ifdef _WIN32
#include <commdlg.h>
#endif

#include <cstdio>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

bool ro::guild::SendEmblemUpload(CGameMode& mode, const QImage& image, QString& error)
{
    const auto& guild = g_session.m_localGuild;
    if (!guild.guildId || !guild.isMaster) {
        error = QStringLiteral("Only the guild master can upload an emblem.");
        return false;
    }
    const auto packet = EmblemUploadPacket(image, error);
    if (packet.isEmpty()) return false;
    if (!CRagConnection::instance()->SendPacket(packet.constData(), static_cast<int>(packet.size()))) {
        error = QStringLiteral("Unable to upload guild emblem. Please check your connection.");
        return false;
    }
    mode.m_pendingEmblemGuild = guild.guildId;
    mode.m_pendingEmblemVersion = guild.emblemVersion;
    mode.m_pendingEmblemTick = GetTickCount();
    mode.m_pendingEmblemData.assign(packet.begin() + 4, packet.end());
    DbgLog("[GuildEmblem] request guild=%u bytes=%d version=%u\n",
        guild.guildId, static_cast<int>(packet.size()), guild.emblemVersion);
    return true;
}

namespace {

void RequestGuildEmblemUpload()
{
    const auto guildId = g_session.m_localGuild.guildId;
    const auto characterId = g_session.m_gid;
    if (!guildId || !g_session.m_localGuild.isMaster) return;
#ifdef _WIN32
    // The main loop owns socket pumping and heartbeats. A synchronous file
    // dialog here suspends both for as long as the user browses their files.
    const bool started=ro::guild::PickEmblemAsync(QCoreApplication::instance(), [] {
        const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        wchar_t path[32768]{};
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        // No cross-thread owner: disabling the game window would also interrupt
        // native input while the independent picker is open.
        dialog.hwndOwner = nullptr;
        dialog.lpstrFilter = L"Guild emblems (24 x 24 BMP or PNG)\0*.bmp;*.png\0\0";
        dialog.lpstrFile = path;
        dialog.nMaxFile = static_cast<DWORD>(std::size(path));
        dialog.lpstrTitle = L"Choose guild emblem (24 x 24 pixels)";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
        const bool selected=GetOpenFileNameW(&dialog)!=FALSE;
        const bool failed=!selected && CommDlgExtendedError()!=0;
        if(SUCCEEDED(initialized))CoUninitialize();
        return ro::guild::EmblemSelection{selected?QString::fromWCharArray(path):QString(),failed};
    }, [guildId,characterId](const ro::guild::EmblemSelection& selection) {
        if (selection.failed) {g_windowMgr.PushChatEvent("Unable to open the guild emblem picker.",0x00FF4040u,6);return;}
        if (selection.path.isEmpty()) return; // Cancel sends nothing.
        if (g_session.m_gid!=characterId || !g_session.m_localGuild.isMaster || g_session.m_localGuild.guildId!=guildId) return;
        auto* mode=g_modeMgr.GetCurrentGameMode();
        if (!mode || !CRagConnection::instance()->IsOpen()) {
            g_windowMgr.PushChatEvent("Guild emblem was not uploaded: connection lost. Please reconnect and try again.",0x00FF4040u,6);
            return;
        }
        QFile file(selection.path);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) {
            g_windowMgr.PushChatEvent("Unable to read emblem. Choose a BMP or PNG smaller than 1 MB.", 0x00FF4040u, 6);
            return;
        }
        QImageReader reader(&file);
        const auto format = reader.format().toLower();
        if ((format != "bmp" && format != "png") || reader.size() != QSize(24, 24)) {
            g_windowMgr.PushChatEvent("Choose a 24 x 24 pixel BMP or PNG guild emblem.", 0x00FF4040u, 6);
            return;
        }
        QString error;
        const QImage image = reader.read();
        if (g_session.m_gid != characterId || g_session.m_localGuild.guildId != guildId) return;
        if (!ro::guild::SendEmblemUpload(*mode, image, error)) {
            g_windowMgr.PushChatEvent(error.toUtf8().constData(), 0x00FF4040u, 6);
            return;
        }
        g_windowMgr.PushChatEvent("Guild emblem request sent; waiting for server confirmation.", 0x0080C0FFu, 6);
    });
    if(!started)g_windowMgr.PushChatEvent("The guild emblem picker is already open.",0x0080C0FFu,6);
#else
        g_windowMgr.PushChatEvent("Guild emblem file selection is unavailable on this platform.", 0x00FF4040u, 6);
#endif
}

constexpr DWORD kHoverNameRequestCooldownMs = 1000;
constexpr int kQtActorLabelVerticalOffset = 10;
constexpr int kQtSpeechBubbleMaxTextWidth = 200;
constexpr int kQtStatusIconSize = 56;
constexpr int kQtStatusIconSpacing = 6;
constexpr int kQtStatusIconTopMargin = 6;

bool IsGameplayUiSuppressed(const QtUiState* state)
{
    return state && state->loadingVisible();
}

QString BuildWallpaperRevisionText()
{
    if (!g_windowMgr.m_loadedWallpaperPath.empty()) {
        return QString::fromLocal8Bit(g_windowMgr.m_loadedWallpaperPath.c_str());
    }
    return QStringLiteral("none");
}

template <typename TWindow>
bool IsGameplayWindowVisible(const QtUiState* state, const TWindow* window)
{
    return window && window->m_show != 0 && !IsGameplayUiSuppressed(state);
}

void ClearGameplayUiState(QtUiState* state)
{
    if (!state) {
        return;
    }

    state->setNpcMenuVisible(false);
    state->setNpcMenuGeometry(0, 0, 0, 0);
    state->setNpcMenuSelection(-1);
    state->setNpcMenuHoverIndex(-1);
    state->setNpcMenuButtons(false, false);
    state->setNpcMenuOptions(QVariantList{});
    state->setNpcMenuButtonsData(QVariantList{});

    state->setSayDialogVisible(false);
    state->setSayDialogGeometry(0, 0, 0, 0);
    state->setSayDialogText(QString());
    state->setSayDialogAction(false, QString(), false, false);
    state->setSayDialogActionButton(QVariantMap{});

    state->setNpcInputVisible(false);
    state->setNpcInputGeometry(0, 0, 0, 0);
    state->setNpcInputText(QString(), QString());
    state->setNpcInputButtons(false, false);
    state->setNpcInputButtonsData(QVariantList{});

    state->setChooseMenuVisible(false);
    state->setChooseMenuGeometry(0, 0, 0, 0);
    state->setChooseMenuSelectedIndex(-1);
    state->setChooseMenuPressedIndex(-1);
    state->setChooseMenuOptions(QVariantList{});

    state->setItemShopVisible(false);
    state->setItemShopGeometry(0, 0, 0, 0);
    state->setItemShopTitle(QString());
    state->setItemShopData(QVariantMap{});
    state->setItemShopRows(QVariantList{});

    state->setItemPurchaseVisible(false);
    state->setItemPurchaseGeometry(0, 0, 0, 0);
    state->setItemPurchaseTotal(QStringLiteral("0"));
    state->setItemPurchaseData(QVariantMap{});
    state->setItemPurchaseRows(QVariantList{});
    state->setItemPurchaseButtons(QVariantList{});

    state->setItemSellVisible(false);
    state->setItemSellGeometry(0, 0, 0, 0);
    state->setItemSellTotal(QStringLiteral("0"));
    state->setItemSellData(QVariantMap{});
    state->setItemSellRows(QVariantList{});
    state->setItemSellButtons(QVariantList{});

    state->setShortCutVisible(false);
    state->setShortCutGeometry(0, 0, 0, 0);
    state->setShortCutPage(0);
    state->setShortCutRows(1);
    state->setShortCutHoverSlot(-1);
    state->setShortCutSlots(QVariantList{});

    state->setBasicInfoVisible(false);
    state->setBasicInfoGeometry(0, 0, 0, 0);
    state->setBasicInfoMini(false);
    state->setBasicInfoData(QVariantMap{});

    state->setStatusVisible(false);
    state->setStatusGeometry(0, 0, 0, 0);
    state->setStatusMini(false);
    state->setStatusData(QVariantMap{});

    state->setChatWindowVisible(false);
    state->setChatWindowGeometry(0, 0, 0, 0);
    state->setChatWindowInputActive(false);
    state->setChatWindowWhisperInputActive(false);
    state->setChatWindowMessageInputActive(false);
    state->setChatWindowWhisperTargetText(QString());
    state->setChatWindowInputText(QString());
    state->setChatWindowLines(QVariantList{});
    state->setChatWindowScrollBar(QVariantMap{});
    state->setChatWindowUi(QVariantMap{});

    state->setRechargeGaugeVisible(false);
    state->setRechargeGaugeGeometry(0, 0, 0, 0);
    state->setRechargeGaugeProgress(0, 0);

    state->setInventoryVisible(false);
    state->setCommerceData({});
    state->setInventoryGeometry(0, 0, 0, 0);
    state->setInventoryMini(false);
    state->setInventoryTab(0);
    state->setInventoryData(QVariantMap{});

    state->setStorageVisible(false);
    state->setStorageGeometry(0, 0, 0, 0);
    state->setStorageMini(false);
    state->setStorageTab(0);
    state->setStorageData(QVariantMap{});

    state->setFriendPartyVisible(false);
    state->setGuildWindow(QVariantMap{});
    state->setFriendPartyGeometry(0, 0, 0, 0);
    state->setFriendPartyData(QVariantMap{});

    state->setPartySetupVisible(false);
    state->setPartySetupGeometry(0, 0, 0, 0);
    state->setPartySetupData(QVariantMap{});

    state->setPartyInviteVisible(false);
    state->setPartyInviteGeometry(0, 0, 0, 0);
    state->setPartyInviteData(QVariantMap{});

    state->setEquipVisible(false);
    state->setEquipGeometry(0, 0, 0, 0);
    state->setEquipMini(false);
    state->setEquipData(QVariantMap{});

    state->setSkillListVisible(false);
    state->setSkillListGeometry(0, 0, 0, 0);
    state->setSkillListData(QVariantMap{});

    state->setItemInfoVisible(false);
    state->setItemInfoGeometry(0, 0, 0, 0);
    state->setItemInfoData(QVariantMap{});
    state->setCardInfoData(QVariantMap{});
    state->setItemObtainData(QVariantMap{});

    state->setSkillDescribeVisible(false);
    state->setSkillDescribeGeometry(0, 0, 0, 0);
    state->setSkillDescribeData(QVariantMap{});

    state->setItemCollectionVisible(false);
    state->setItemCollectionGeometry(0, 0, 0, 0);
    state->setItemCollectionData(QVariantMap{});

    state->setItemIdentifyVisible(false);
    state->setItemIdentifyGeometry(0, 0, 0, 0);
    state->setItemIdentifyData(QVariantMap{});

    state->setSkillSelectVisible(false);
    state->setSkillSelectGeometry(0, 0, 0, 0);
    state->setSkillSelectData(QVariantMap{});

    state->setItemCompositionVisible(false);
    state->setItemCompositionGeometry(0, 0, 0, 0);
    state->setItemCompositionData(QVariantMap{});

    state->setOptionVisible(false);
    state->setOptionGeometry(0, 0, 0, 0);
    state->setOptionData(QVariantMap{});

    state->setMinimapVisible(false);
    state->setMinimapGeometry(0, 0, 0, 0);
    state->setMinimapData(QVariantMap{});
    state->setStatusIcons(QVariantList{});

    state->setShopChoiceVisible(false);
    state->setShopChoiceGeometry(0, 0, 0, 0);
    state->setShopChoiceText(QString(), QString());
    state->setShopChoiceButtons(QVariantList{});

    state->setNotifications(QVariantList{});
    state->setAnchors(QVariantList{});
}

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

QString FormatBasicGaugeText(const char* label, long long current, long long maximum)
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

QString FormatGroupedNumber(int value)
{
    const bool negative = value < 0;
    const QString digits = QString::number(negative ? -value : value);
    QString grouped;
    for (int index = 0; index < digits.size(); ++index) {
        if (index != 0 && (digits.size() - index) % 3 == 0) {
            grouped += QLatin1Char(',');
        }
        grouped += digits.at(index);
    }
    if (negative) {
        grouped.prepend(QLatin1Char('-'));
    }
    return grouped;
}

QString FormatBasicInfoMiniStatusText(long long hp, long long maxHp, long long sp, long long maxSp, int money)
{
    return QStringLiteral("HP %1 / %2  |  SP %3 / %4  |  %5 Z")
        .arg(hp)
        .arg(maxHp)
        .arg(sp)
        .arg(maxSp)
        .arg(FormatGroupedNumber(money));
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

QString ResolveStatusIconLabel(int statusType, int skillId)
{
    if (skillId > 0) {
        if (const SkillMetadata* metadata = g_skillMgr.GetSkillMetadata(skillId)) {
            if (!metadata->displayName.empty()) {
                return ToQString(metadata->displayName);
            }
            if (!metadata->skillIdName.empty()) {
                return ToQString(metadata->skillIdName);
            }
        }
    }

    if (const qtui::StatusIconCatalogEntry* entry = qtui::FindStatusIconCatalogEntry(statusType)) {
        return ToQString(entry->fallbackLabel);
    }

    return QStringLiteral("Status %1").arg(statusType);
}

QString FormatStatusDuration(u32 remainingMs)
{
    const u32 totalSeconds = (remainingMs + 999u) / 1000u;
    const u32 seconds = totalSeconds % 60u;
    const u32 totalMinutes = totalSeconds / 60u;
    const u32 hours = totalMinutes / 60u;
    const u32 minutes = totalMinutes % 60u;

    if (hours > 0u) {
        return QStringLiteral("%1:%2:%3")
            .arg(static_cast<int>(hours))
            .arg(static_cast<int>(minutes), 2, 10, QChar('0'))
            .arg(static_cast<int>(seconds), 2, 10, QChar('0'));
    }

    return QStringLiteral("%1:%2")
        .arg(static_cast<int>(totalMinutes))
        .arg(static_cast<int>(seconds), 2, 10, QChar('0'));
}

QString BuildStatusIconTooltipText(const QVariantMap& icon)
{
    const QString name = icon.value(QStringLiteral("name")).toString();
    if (icon.value(QStringLiteral("timed")).toBool()) {
        const int remainingMs = (std::max)(0, icon.value(QStringLiteral("remainingMs")).toInt());
        return QStringLiteral("%1\n%2 remaining").arg(name, FormatStatusDuration(static_cast<u32>(remainingMs)));
    }
    return QStringLiteral("%1\nActive").arg(name);
}

int FindHoveredStatusIconIndex(const QtUiState* state, int mouseX, int mouseY)
{
    if (!state || !state->minimapVisible()) {
        return -1;
    }

    const QVariantList& icons = state->statusIcons();
    if (icons.isEmpty()) {
        return -1;
    }

    const int stackX = state->minimapX() + (std::max)(0, state->minimapWidth() - kQtStatusIconSize);
    const int stackY = state->minimapY() + state->minimapHeight() + kQtStatusIconTopMargin;
    const int stride = kQtStatusIconSize + kQtStatusIconSpacing;
    const int stackHeight = (icons.size() * kQtStatusIconSize) + ((icons.size() - 1) * kQtStatusIconSpacing);
    if (mouseX < stackX || mouseX >= stackX + kQtStatusIconSize || mouseY < stackY || mouseY >= stackY + stackHeight) {
        return -1;
    }

    const int localY = mouseY - stackY;
    const int index = localY / stride;
    if (index < 0 || index >= icons.size()) {
        return -1;
    }
    if ((localY % stride) >= kQtStatusIconSize) {
        return -1;
    }
    return index;
}

bool TryAppendHoveredStatusIconAnchor(const QtUiState* state, int mouseX, int mouseY, QVariantList* anchors)
{
    if (!state || !anchors) {
        return false;
    }

    const int hoveredIndex = FindHoveredStatusIconIndex(state, mouseX, mouseY);
    if (hoveredIndex < 0) {
        return false;
    }

    const QVariantList& icons = state->statusIcons();
    if (hoveredIndex >= icons.size()) {
        return false;
    }

    const QVariantMap icon = icons[hoveredIndex].toMap();
    const int logicalCenterX = state->minimapX() + (std::max)(0, state->minimapWidth() - kQtStatusIconSize) + (kQtStatusIconSize / 2);
    const int logicalBottomY = state->minimapY()
        + state->minimapHeight()
        + kQtStatusIconTopMargin
        + (hoveredIndex * (kQtStatusIconSize + kQtStatusIconSpacing))
        - 8;

    QVariantMap anchor;
    anchor.insert(QStringLiteral("text"), BuildStatusIconTooltipText(icon));
    anchor.insert(QStringLiteral("centerX"), UiScaleLogicalToRawCoordinate(logicalCenterX));
    anchor.insert(QStringLiteral("bottomY"), UiScaleLogicalToRawCoordinate(logicalBottomY));
    anchor.insert(QStringLiteral("background"), QStringLiteral("#ea1d232c"));
    anchor.insert(QStringLiteral("foreground"), QStringLiteral("#f3f4f6"));
    anchor.insert(QStringLiteral("showBubble"), true);
    anchor.insert(QStringLiteral("fontPixelSize"), 11);
    anchor.insert(QStringLiteral("bold"), false);
    anchor.insert(QStringLiteral("wrap"), true);
    anchor.insert(QStringLiteral("maxTextWidth"), 180);
    anchor.insert(QStringLiteral("paddingX"), 16);
    anchor.insert(QStringLiteral("paddingY"), 10);
    anchor.insert(QStringLiteral("radius"), 5);
    anchor.insert(QStringLiteral("borderColor"), QStringLiteral("#9ea9b8"));
    anchor.insert(QStringLiteral("z"), 2100);
    anchors->push_back(anchor);
    return true;
}

QString BuildStatusIconShortLabel(const QString& label)
{
    const QString simplified = label.simplified();
    if (simplified.isEmpty()) {
        return QStringLiteral("?");
    }

    const QStringList parts = simplified.split(QChar(' '), Qt::SkipEmptyParts);
    QString shortLabel;
    for (const QString& part : parts) {
        if (part.isEmpty()) {
            continue;
        }
        shortLabel += part.left(1).toUpper();
        if (shortLabel.size() >= 2) {
            break;
        }
    }

    if (shortLabel.isEmpty()) {
        shortLabel = simplified.left(2).toUpper();
    }

    return shortLabel.left(3);
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
    loginPanelLabels.insert(QStringLiteral("title"), QStringLiteral("OpenMidgard"));
    loginPanelLabels.insert(QStringLiteral("saveLabel"), QStringLiteral("Remember account ID"));
    const auto layout = loginWnd->GetQtLayout();
    const auto rectangle = [](const loginLayout::Rect& rect) {
        return QVariantMap{{"x", rect.x}, {"y", rect.y}, {"width", rect.width}, {"height", rect.height}};
    };
    loginPanelLabels.insert(QStringLiteral("compact"), layout.compact);
    loginPanelLabels.insert(QStringLiteral("account"), rectangle(layout.account));
    loginPanelLabels.insert(QStringLiteral("password"), rectangle(layout.password));
    loginPanelLabels.insert(QStringLiteral("remember"), rectangle(layout.remember));
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

    if (IsGameplayUiSuppressed(state)) {
        state->setNotifications(QVariantList{});
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

    const UISelectCartWnd* const cartWnd = g_windowMgr.m_selectCartWnd;
    const bool cartVisible = IsGameplayWindowVisible(state, cartWnd);
    if (cartVisible) {
        state->setShopChoiceVisible(true);
        state->setShopChoiceGeometry(cartWnd->m_x, cartWnd->m_y, cartWnd->m_w, cartWnd->m_h);
        state->setShopChoiceText(QStringLiteral("Select Cart"), QStringLiteral("Choose the pushcart style to equip."));

        QVariantList buttons;
        const int optionCount = cartWnd->GetVisibleOptionCount();
        for (int index = 0; index < optionCount; ++index) {
            const RECT rect = cartWnd->GetOptionRectForRender(index);
            QVariantMap button;
            button.insert(QStringLiteral("label"), QStringLiteral("Cart %1").arg(index + 1));
            button.insert(QStringLiteral("x"), QVariant::fromValue(static_cast<int>(rect.left)));
            button.insert(QStringLiteral("y"), QVariant::fromValue(static_cast<int>(rect.top)));
            button.insert(QStringLiteral("width"), QVariant::fromValue(static_cast<int>(rect.right - rect.left)));
            button.insert(QStringLiteral("height"), QVariant::fromValue(static_cast<int>(rect.bottom - rect.top)));
            button.insert(QStringLiteral("hot"), cartWnd->GetHoverIndex() == index);
            button.insert(QStringLiteral("pressed"), cartWnd->GetPressedIndex() == index);
            buttons.push_back(button);
        }
        state->setShopChoiceButtons(buttons);
        return;
    }

    const UIChooseSellBuyWnd* const shopWnd = g_windowMgr.m_chooseSellBuyWnd;
    const bool visible = IsGameplayWindowVisible(state, shopWnd);
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

    const UIPlayerContextMenuWnd* const playerMenuWnd = g_windowMgr.m_playerContextMenuWnd;
    const bool playerMenuVisible = false; // Rendered separately by RoCommerce.
    if (playerMenuVisible) {
        state->setNpcMenuVisible(true);
        state->setNpcMenuGeometry(playerMenuWnd->m_x, playerMenuWnd->m_y, playerMenuWnd->m_w, playerMenuWnd->m_h);
        state->setNpcMenuSelection(playerMenuWnd->GetSelectedIndex());
        state->setNpcMenuHoverIndex(playerMenuWnd->GetHoverIndex());
        state->setNpcMenuButtons(false, false);

        QVariantList options;
        const std::vector<std::string>& rawOptions = playerMenuWnd->GetOptions();
        options.reserve(static_cast<qsizetype>(rawOptions.size()));
        for (const std::string& option : rawOptions) {
            options.push_back(NpcColorCodesToHtml(option));
        }
        state->setNpcMenuOptions(options);
        state->setNpcMenuButtonsData(QVariantList{});
        return;
    }

    const UINpcMenuWnd* const menuWnd = g_windowMgr.m_npcMenuWnd;
    const bool visible = IsGameplayWindowVisible(state, menuWnd);
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
    const bool visible = IsGameplayWindowVisible(state, dialogWnd);
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
    const bool visible = IsGameplayWindowVisible(state, inputWnd);
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
        ToQString(inputWnd->GetDialogLabel()),
        ToQString(inputWnd->GetInputText()),
        ToQString(inputWnd->GetInputPlaceholder()));
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

    UIChooseWnd* const chooseWnd = g_windowMgr.m_chooseWnd;
    const bool visible = IsGameplayWindowVisible(state, chooseWnd);
    state->setChooseMenuVisible(visible);
    if (!visible) {
        state->setChooseMenuGeometry(0, 0, 0, 0);
        state->setChooseMenuSelectedIndex(-1);
        state->setChooseMenuPressedIndex(-1);
        state->setChooseMenuOptions(QVariantList{});
        return;
    }

    chooseWnd->RefreshRecoveryOption();
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
    const bool visible = IsGameplayWindowVisible(state, shopWnd);
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
        entry.insert(QStringLiteral("itemId"), row.itemInfo.GetItemId());
        entry.insert(QStringLiteral("identified"), row.itemInfo.m_isIdentified != 0);
        entry.insert(QStringLiteral("quantity"), row.availableCount);
        entry.insert(QStringLiteral("price"), QString::number(g_session.GetNpcShopUnitPrice(row)));
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
    const bool visible = IsGameplayWindowVisible(state, purchaseWnd);
    state->setItemPurchaseVisible(visible);
    if (!visible) {
        state->setItemPurchaseGeometry(0, 0, 0, 0);
        state->setItemPurchaseTotal(QStringLiteral("0"));
        state->setItemPurchaseData(QVariantMap{});
        state->setItemPurchaseRows(QVariantList{});
        state->setItemPurchaseButtons(QVariantList{});
        return;
    }

    state->setItemPurchaseGeometry(purchaseWnd->m_x, purchaseWnd->m_y, purchaseWnd->m_w, purchaseWnd->m_h);
    state->setItemPurchaseTotal(QString::number(g_session.m_shopDealTotal));
    QVariantMap purchaseData;
    purchaseData.insert(QStringLiteral("title"), QStringLiteral("Purchase"));
    purchaseData.insert(QStringLiteral("nameLabel"), QStringLiteral("Item"));
    purchaseData.insert(QStringLiteral("quantityLabel"), QStringLiteral("Qty"));
    purchaseData.insert(QStringLiteral("amountLabel"), QStringLiteral("Cost"));
    purchaseData.insert(QStringLiteral("totalLabel"), QStringLiteral("Total"));
    if (g_session.m_shopUsesCash) {
        purchaseData.insert(QStringLiteral("balanceLabel"),
            QStringLiteral("Cash %1  Kafra %2").arg(g_session.m_cashShopPoints).arg(g_session.m_kafraShopPoints));
    }
    state->setItemPurchaseData(purchaseData);

    QVariantList rows;
    const int startRow = purchaseWnd->GetViewOffset();
    const int endRow = (std::min)(
        static_cast<int>(g_session.m_shopDealRows.size()),
        startRow + purchaseWnd->GetVisibleRowCountForQt());
    rows.reserve((std::max)(0, endRow - startRow));
    for (int rowIndex = startRow; rowIndex < endRow; ++rowIndex) {
        const NPC_SHOP_DEAL_ROW& row = g_session.m_shopDealRows[static_cast<size_t>(rowIndex)];
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), ToQString(shopui::GetItemDisplayName(row.itemInfo)));
        entry.insert(QStringLiteral("itemId"), row.itemInfo.GetItemId());
        entry.insert(QStringLiteral("identified"), row.itemInfo.m_isIdentified != 0);
        entry.insert(QStringLiteral("quantity"), row.quantity);
        entry.insert(QStringLiteral("cost"), QString::number(static_cast<unsigned long long>(row.unitPrice) * row.quantity));
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
    const bool visible = IsGameplayWindowVisible(state, sellWnd);
    state->setItemSellVisible(visible);
    if (!visible) {
        state->setItemSellGeometry(0, 0, 0, 0);
        state->setItemSellTotal(QStringLiteral("0"));
        state->setItemSellData(QVariantMap{});
        state->setItemSellRows(QVariantList{});
        state->setItemSellButtons(QVariantList{});
        return;
    }

    state->setItemSellGeometry(sellWnd->m_x, sellWnd->m_y, sellWnd->m_w, sellWnd->m_h);
    state->setItemSellTotal(QString::number(g_session.m_shopDealTotal));
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
        startRow + sellWnd->GetVisibleRowCountForQt());
    rows.reserve((std::max)(0, endRow - startRow));
    for (int rowIndex = startRow; rowIndex < endRow; ++rowIndex) {
        const NPC_SHOP_DEAL_ROW& row = g_session.m_shopDealRows[static_cast<size_t>(rowIndex)];
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), ToQString(shopui::GetItemDisplayName(row.itemInfo)));
        entry.insert(QStringLiteral("itemId"), row.itemInfo.GetItemId());
        entry.insert(QStringLiteral("identified"), row.itemInfo.m_isIdentified != 0);
        entry.insert(QStringLiteral("quantity"), row.quantity);
        entry.insert(QStringLiteral("gain"), QString::number(static_cast<unsigned long long>(row.unitPrice) * row.quantity));
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
    const bool visible = IsGameplayWindowVisible(state, shortCutWnd);
    state->setShortCutVisible(visible);
    if (!visible) {
        state->setShortCutGeometry(0, 0, 0, 0);
        state->setShortCutPage(0);
        state->setShortCutRows(1);
        state->setShortCutHoverSlot(-1);
        state->setShortCutSlots(QVariantList{});
        return;
    }

    state->setShortCutGeometry(shortCutWnd->m_x, shortCutWnd->m_y, shortCutWnd->m_w, shortCutWnd->m_h);
    state->setShortCutPage(g_session.GetShortcutPage() + 1);
    state->setShortCutRows(shortCutWnd->GetVisibleRowCount());
    state->setShortCutHoverSlot(shortCutWnd->GetHoverSlot());

    QVariantList shortcutSlots;
    shortcutSlots.reserve(kShortcutSlotCount);
    for (int slotIndex = 0; slotIndex < kShortcutSlotCount; ++slotIndex) {
        const int row = slotIndex / kShortcutSlotsPerPage;
        const int column = slotIndex % kShortcutSlotsPerPage;
        const int page = row;
        const int absoluteIndex = page * kShortcutSlotsPerPage + column;
        QVariantMap slotEntry;
        slotEntry.insert(QStringLiteral("index"), slotIndex);
        slotEntry.insert(QStringLiteral("row"), row);
        slotEntry.insert(QStringLiteral("column"), column);
        slotEntry.insert(QStringLiteral("hover"), shortCutWnd->GetHoverSlot() == slotIndex);

        const SHORTCUT_SLOT* slot = g_session.GetShortcutSlotByAbsoluteIndex(absoluteIndex);
        if (slot && slot->id != 0) {
            slotEntry.insert(QStringLiteral("occupied"), true);
            slotEntry.insert(QStringLiteral("isSkill"), slot->isSkill != 0);
            slotEntry.insert(QStringLiteral("skillId"), slot->isSkill != 0 ? static_cast<int>(slot->id) : 0);
            slotEntry.insert(QStringLiteral("itemId"), slot->isSkill != 0 ? 0u : slot->id);
            slotEntry.insert(QStringLiteral("label"), ResolveShortcutSlotLabel(slot));
            int count = static_cast<int>(slot->count);
            if (slot->isSkill) {
                const auto* learned = g_session.GetSkillItemBySkillId(static_cast<int>(slot->id));
                count = learned && learned->level > 0 ? std::clamp(count > 0 ? count : learned->level, 1, learned->level) : 0;
            }
            slotEntry.insert(QStringLiteral("count"), count);
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
    const bool visible = IsGameplayWindowVisible(state, basicInfoWnd);
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
            QStringLiteral("Zeny : %1").arg(FormatGroupedNumber(display.money)));

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
            button.insert(QStringLiteral("hovered"), buttonDisplay.hovered);
            button.insert(QStringLiteral("pressed"), buttonDisplay.pressed);
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
            button.insert(QStringLiteral("icon"), ToQString(buttonDisplay.icon));
            button.insert(QStringLiteral("hovered"), buttonDisplay.hovered);
            button.insert(QStringLiteral("pressed"), buttonDisplay.pressed);
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
    const bool visible = IsGameplayWindowVisible(state, statusWnd);
    state->setStatusVisible(visible);
    if (!visible) {
        state->setStatusGeometry(0, 0, 0, 0);
        state->setStatusMini(false);
        state->setStatusData(QVariantMap{});
        return;
    }

    state->setStatusGeometry(statusWnd->m_x, statusWnd->m_y, statusWnd->m_w, statusWnd->m_h);
    state->setStatusMini(statusWnd->IsMiniMode());

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
            row.insert(QStringLiteral("base"), display.baseStats[index]);
            row.insert(QStringLiteral("bonus"), display.plusStats[index]);
            row.insert(QStringLiteral("rowY"), statusLayout::rowY(index));
            UIStatusWnd::QtButtonDisplay incrementButton{};
            const bool hasIncrementButton = statusWnd->GetQtIncrementButtonDisplayForQt(index, &incrementButton);
            row.insert(QStringLiteral("canIncrease"), hasIncrementButton && incrementButton.visible && incrementButton.active);
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

double SourceManagedWindowLayer(const UIWindow* window)
{
    int index=0,mainIndex=0,windowIndex=0;
    for(const auto* child:g_windowMgr.m_children){if(child==g_windowMgr.m_chatWnd)mainIndex=index;if(child==window)windowIndex=index;++index;}
    return (windowIndex-mainIndex)*0.0001;
}
void PopulateChatAuxiliaryState(QtUiState* state, QVariantMap& chatUi)
{
    const auto chatLayer=SourceManagedWindowLayer;
    QVariantMap friendOptions;
    if(const auto* window=g_windowMgr.m_friendOptionWnd){
        QVariantList checks;
        for(int i=0;i<3;++i){const auto* row=window->GetCheckBox(i);const auto& bitmap=row->GetBitmap();checks.push_back(QVariantMap{
            {"x",row->m_x-window->m_x},{"y",row->m_y-window->m_y},{"width",row->m_w},{"height",row->m_h},{"label",ToQString(row->GetLabel())},
            {"checked",row->IsChecked()},{"artwork",QString::fromStdString(row->GetBitmapUri())},{"imageWidth",bitmap.width},{"imageHeight",bitmap.height}});}
        const auto* close=window->GetCloseButton();const int status=close->GetVisualState();const auto& bitmap=close->GetButton().bitmaps[status];
        friendOptions={{"visible",IsGameplayWindowVisible(state,window)},{"layer",chatLayer(window)},{"x",window->m_x},{"y",window->m_y},{"width",window->m_w},{"height",window->m_h},
            {"caption",ToQString(window->GetCaption())},{"checks",checks},{"close",QVariantMap{{"x",close->m_x-window->m_x},{"y",close->m_y-window->m_y},{"width",close->m_w},{"height",close->m_h},
                {"artwork",QString::fromStdString(close->GetButton().uris[status])},{"imageWidth",bitmap.width},{"imageHeight",bitmap.height},{"state",status}}}};
    }
    chatUi.insert("friendOptions",friendOptions);
    QVariantMap autoMessage;
    if(const auto* window=g_windowMgr.m_autoMessageWnd){
        const auto& button=window->GetButton();const auto bounds=button.Bounds();const int status=window->GetButtonState();const auto& bitmap=button.bitmaps[status];
        autoMessage={{"visible",IsGameplayWindowVisible(state,window)},{"layer",chatLayer(window)},{"x",window->m_x},{"y",window->m_y},
            {"width",window->m_w},{"height",window->m_h},{"prompt",ToQString(window->GetPrompt())},{"input",ToQString(window->GetInputText())},{"selected",window->IsSelected()},
            {"button",QVariantMap{{"x",button.x},{"y",button.y},{"width",static_cast<int>(bounds.right-bounds.left)},{"height",static_cast<int>(bounds.bottom-bounds.top)},
                {"artwork",QString::fromStdString(button.uris[status])},{"imageWidth",bitmap.width},{"imageHeight",bitmap.height},{"pressed",status==2}}}};
    }
    chatUi.insert("autoMessage",autoMessage);
    QVariantMap tipData;
    if(const auto* window=dynamic_cast<UITipOfTheDayWnd*>(g_windowMgr.m_tipOfTheDayWnd)){
        QVariantList buttons,rows;const auto* viewer=window->GetViewer();const auto bar=viewer->GetScrollBar();
        for(const auto* button:window->GetButtons()){const int status=button->GetVisualState();const auto& art=button->GetButton();const auto& bitmap=art.bitmaps[status];
            buttons.push_back(QVariantMap{{"id",button->m_id},{"x",button->m_x-window->m_x},{"y",button->m_y-window->m_y},
                {"width",button->m_w},{"height",button->m_h},{"label",ToQString(button->m_text)},{"state",status},
                {"artwork",QString::fromStdString(art.uris[status])},{"imageWidth",bitmap.width},{"imageHeight",bitmap.height}});}
        for(int i=viewer->GetOffset();i<static_cast<int>(viewer->GetRows().size()) && i<viewer->GetOffset()+bar.page;++i){QVariantList spans;
            for(const auto& span:viewer->GetRows()[i])spans.push_back(QVariantMap{{"text",ToQString(span.text)},{"color",QString("#%1").arg(span.color,6,16,QChar('0'))}});
            rows.push_back(QVariantMap{{"spans",spans}});}
        const auto* check=window->GetStartupCheckBox();const auto& checkArt=check->GetBitmap();
        const auto rect=[](RECT r){return QVariantMap{{"x",int(r.left)},{"y",int(r.top)},{"width",int(r.right-r.left)},{"height",int(r.bottom-r.top)}};};
        const RECT rail{viewer->m_w-13,0,viewer->m_w,viewer->m_h};const auto layout=ro::chat::ScrollLayout(rail,bar);
        tipData={{"visible",IsGameplayWindowVisible(state,window)},{"layer",chatLayer(window)},{"x",window->m_x},{"y",window->m_y},{"width",window->m_w},{"height",window->m_h},
            {"rows",rows},{"buttons",buttons},{"scroll",QVariantMap{{"visible",bar.maximum>0},{"track",rect(rail)},{"thumb",rect(layout.thumb)},{"up",rect(layout.up)},{"down",rect(layout.down)}}},
            {"check",QVariantMap{{"x",check->m_x-window->m_x},{"y",check->m_y-window->m_y},{"width",check->m_w},{"height",check->m_h},{"label",ToQString(check->GetLabel())},
                {"checked",check->IsChecked()},{"artwork",QString::fromStdString(check->GetBitmapUri())},{"imageWidth",checkArt.width},{"imageHeight",checkArt.height}}}};
    }
    chatUi.insert("tip",tipData);
    QVariantMap commandList;
    if(const auto* window=dynamic_cast<const UICommandListWnd*>(g_windowMgr.m_commandListWnd)){
        QVariantList framePieces,rows,buttons;
        for(const auto& piece:window->GetFramePieces())
            framePieces.push_back(QVariantMap{{"x",piece.x},{"y",piece.y},{"width",piece.width},{"height",piece.height},{"phase",piece.phase},
                {"artwork",QString::fromStdString(piece.uri)}});
        for(const auto& row:window->GetVisibleRows())
            rows.push_back(QVariantMap{{"index",row.index},{"value",row.value},{"x",row.x},{"y",row.y},{"width",row.width},
                {"height",row.height},{"textOffset",row.textOffset},{"caption",ToQString(row.caption)},
                {"selected",row.selected},{"hasIndicator",row.hasIndicator},{"on",row.on}});
        const auto* tab=window->GetTab();
        const auto& tabBitmap=tab->GetBitmap();
        QVariantMap tabData{{"x",tab->m_x-window->m_x},{"y",tab->m_y-window->m_y},{"width",tab->m_w},{"height",tab->m_h},
            {"artwork",QString::fromStdString(tab->GetBitmapUri())},{"imageWidth",tabBitmap.width},{"imageHeight",tabBitmap.height}};
        for(const auto* button:window->GetButtons())if(button){
            const int status=button->GetVisualState();const auto& art=button->GetButton();const auto& bitmap=art.bitmaps[status];
            buttons.push_back(QVariantMap{{"id",button->m_id},{"x",button->m_x-window->m_x},{"y",button->m_y-window->m_y},
                {"width",button->m_w},{"height",button->m_h},{"label",ToQString(button->m_text)},
                {"artwork",QString::fromStdString(art.uris[status])},{"imageWidth",bitmap.width},{"imageHeight",bitmap.height},{"state",status}});
        }
        const auto bar=window->GetScrollBar();
        const RECT rail{window->m_w-15,14,window->m_w-2,window->m_h-24};
        const auto layout=ro::chat::ScrollLayout(rail,bar);
        const auto rect=[](RECT r){return QVariantMap{{"x",int(r.left)},{"y",int(r.top)},
            {"width",int(r.right-r.left)},{"height",int(r.bottom-r.top)}};};
        const auto& indicatorOff=window->GetIndicatorBitmap(false);const auto& indicatorOn=window->GetIndicatorBitmap(true);
        commandList={{"visible",IsGameplayWindowVisible(state,window)},{"layer",chatLayer(window)},
            {"x",window->m_x},{"y",window->m_y},{"width",window->m_w},{"height",window->m_h},
            {"title",ToQString(window->GetTitle())},{"framePieces",framePieces},{"rows",rows},{"buttons",buttons},{"tab",tabData},
            {"indicatorOff",QString::fromStdString(window->GetIndicatorUri(false))},
            {"indicatorOffWidth",indicatorOff.width},{"indicatorOffHeight",indicatorOff.height},
            {"indicatorOn",QString::fromStdString(window->GetIndicatorUri(true))},
            {"indicatorOnWidth",indicatorOn.width},{"indicatorOnHeight",indicatorOn.height},
            {"scroll",QVariantMap{{"visible",bar.maximum>0},{"track",rect(rail)},{"thumb",rect(layout.thumb)},
                {"up",rect(layout.up)},{"down",rect(layout.down)}}}};
    }
    chatUi.insert("commandList",commandList);
    QVariantMap sourceHelp;
    if(const auto* mode=g_modeMgr.GetCurrentMode())if(const auto* window=mode->GetHelpBalloon()){
        QVariantList lines;
        for(const auto& line:window->GetDisplayLines())lines.push_back(QString::fromUtf8(line.c_str()));
        const QString fontColor=QString("#%1").arg(static_cast<qulonglong>(window->GetFontColor()&0xffffffu),6,16,QChar('0'));
        sourceHelp={{"visible",IsGameplayWindowVisible(state,window)},{"layer",chatLayer(window)},
            {"x",window->m_x},{"y",window->m_y},{"width",window->m_w},{"height",window->m_h},
            {"lines",lines},{"lineHeight",window->GetLineHeight()},{"fontColor",fontColor}};
    }
    chatUi.insert("sourceHelp",sourceHelp);
    QVariantList whisperWindows;
    const auto whisperRect=[](RECT r){return QVariantMap{{"x",static_cast<int>(r.left)},{"y",static_cast<int>(r.top)},
        {"width",static_cast<int>(r.right-r.left)},{"height",static_cast<int>(r.bottom-r.top)}};};
    for(const auto& entry:g_windowMgr.m_whisperWindows){
        const auto* window=entry.second;const auto layout=window->GetLayout();QVariantList rows;
        for(const auto& row:window->GetVisibleRows())rows.push_back(QVariantMap{{"text",ToQString(row.text)},{"color",QColor::fromRgb(row.color).name()}});
        whisperWindows.push_back(QVariantMap{{"name",ToQString(window->GetName())},{"title",ToQString(window->GetTitle())},
            {"visible",IsGameplayWindowVisible(state,window)},{"layer",chatLayer(window)},{"x",window->m_x},{"y",window->m_y},
            {"width",window->m_w},{"height",window->m_h},{"input",ToQString(window->GetInputText())},{"rows",rows},
            {"history",whisperRect(layout.history)},{"edit",whisperRect(layout.edit)},{"close",whisperRect(layout.close)},
            {"resizer",whisperRect(layout.resizer)},{"scrollTrack",whisperRect(layout.track)},{"scrollThumb",whisperRect(layout.thumb)},
            {"scrollUp",whisperRect(layout.up)},{"scrollDown",whisperRect(layout.down)},{"scrollVisible",window->GetScrollModel().maximum>0}});
    }
    chatUi.insert("whisperWindows",whisperWindows);
    QVariantMap roomMenu;
    if(const auto* menu=g_windowMgr.m_chatRoomMenuWnd){QVariantList rows;
        for(const auto& item:menu->GetItems())rows.push_back(QVariantMap{{"action",item.action},{"text",ToQString(item.label)}});
        roomMenu={{"visible",IsGameplayWindowVisible(state,menu)},{"layer",chatLayer(menu)},{"x",menu->m_x},{"y",menu->m_y},
            {"width",menu->m_w},{"height",menu->m_h},{"hover",menu->GetHoverIndex()},{"rows",rows}};
    }
    chatUi.insert("roomMenu",roomMenu);
    QVariantMap petInfo;
    if (auto* window = g_windowMgr.m_petInfoWnd) {
        QVariantList framePieces, texts, buttons;
        for (const auto& piece : window->GetFramePieces()) {
            framePieces.push_back(QVariantMap{{"x",piece.x},{"y",piece.y},
                {"width",piece.bitmap.width},{"height",piece.bitmap.height},
                {"artwork",QString::fromStdString(piece.uri)},{"phase",piece.phase}});
        }
        for (const auto& row : window->GetDisplayTexts()) {
            const COLORREF color = static_cast<COLORREF>(row.color);
            texts.push_back(QVariantMap{{"x",row.x},{"y",row.y},
                {"text",QString::fromUtf8(row.text.c_str())},
                {"color",QColor(GetRValue(color),GetGValue(color),GetBValue(color)).name()}});
        }
        for (const auto* button : window->GetButtons()) {
            if (!button) continue;
            const int status = button->GetVisualState();
            const auto& art = button->GetButton();
            const auto& bitmap = art.bitmaps[status];
            buttons.push_back(QVariantMap{{"id",button->m_id},
                {"x",button->m_x-window->m_x},{"y",button->m_y-window->m_y},
                {"width",button->m_w},{"height",button->m_h},
                {"artwork",QString::fromStdString(art.uris[status])},
                {"imageWidth",bitmap.width},{"imageHeight",bitmap.height},{"state",status},
                {"pressed",button->IsLogicallyPressed()},{"label",button->m_id==135?QStringLiteral("x"):QStringLiteral("*")}});
        }
        const auto& portrait = window->GetPortrait();
        const UIEditCtrl* edit = window->GetNameEdit();
        const int editX = edit ? edit->m_x-window->m_x : -100;
        const int editY = edit ? edit->m_y-window->m_y : -100;
        const bool editVisible = edit && editX >= 0;
        const int job = g_session.m_petStatus.job;
        petInfo = QVariantMap{{"visible",IsGameplayWindowVisible(state,window)},
            {"layer",SourceManagedWindowLayer(window)},
            {"x",window->m_x},{"y",window->m_y},{"width",window->m_w},{"height",window->m_h},
            {"framePieces",framePieces},{"texts",texts},{"buttons",buttons},
            {"portrait",QVariantMap{{"x",5},{"y",21},{"width",portrait.bitmap.width},{"height",portrait.bitmap.height},
                {"artwork",QString::fromStdString(portrait.uri)}}},
            {"commandBoxVisible",job>=1000 && job<=1985},{"commandBoxColor",QStringLiteral("#e6e6fa")},
            {"edit",QVariantMap{{"visible",editVisible},{"x",editX},
                {"y",editY},{"width",edit?edit->m_w:0},{"height",edit?edit->m_h:0},
                {"text",editVisible?QString::fromUtf8(window->GetEditDisplayText().c_str()):QString()},
                {"focused",edit && g_windowMgr.m_editWindow==edit}}}};
        if (auto* menu = window->GetCommandMenu()) {
            QVariantList rows;
            for (const auto& label : menu->GetLabels()) rows.push_back(QString::fromUtf8(label.c_str()));
            petInfo.insert("menu",QVariantMap{{"visible",menu->m_show!=0},
                {"layer",SourceManagedWindowLayer(menu)},
                {"x",menu->m_x},{"y",menu->m_y},{"width",110},{"height",64},
                {"selection",menu->GetSelection()},{"rows",rows}});
        }
    }
    chatUi.insert("petInfo",petInfo);
    QVariantList companionStatus;
    for (const auto* window : {g_windowMgr.m_homunInfoWnd, g_windowMgr.m_merInfoWnd}) {
        if (!window || !IsGameplayWindowVisible(state, window)) continue;
        const auto display = window->GetDisplay();
        QVariantList texts, gauges, buttons;
        for (const auto& row : display.text) {
            texts.push_back(QVariantMap{{"x", row.x}, {"y", row.y}, {"width", row.width},
                {"text", ToQString(row.value)}, {"rightAligned", row.rightAligned}});
        }
        for (const auto& gauge : display.gauges) {
            gauges.push_back(QVariantMap{{"x", gauge.x}, {"y", gauge.y}, {"width", gauge.width},
                {"height", gauge.height}, {"filledWidth", gauge.filledWidth},
                {"color", QColor(GetRValue(gauge.color), GetGValue(gauge.color), GetBValue(gauge.color)).name()}});
        }
        for (const auto* button : window->GetButtons()) {
            if (!button->m_show) continue;
            const auto& artwork = button->GetButton().uris[button->GetVisualState()];
            buttons.push_back(QVariantMap{{"x", button->m_x - window->m_x}, {"y", button->m_y - window->m_y},
                {"width", button->m_w}, {"height", button->m_h}, {"artwork", ToQString(artwork)},
                {"label", button->m_id == 135 ? QStringLiteral("×") : ToQString(button->m_toolTip)}});
        }
        QVariantMap nameEdit;
        if (const auto* edit = window->GetNameEdit()) {
            nameEdit = {{"visible", true}, {"x", edit->m_x - window->m_x}, {"y", edit->m_y - window->m_y},
                {"width", edit->m_w}, {"height", edit->m_h}, {"text", ToQString(window->GetEditDisplayText())},
                {"focused", g_windowMgr.m_editWindow == edit}};
        }
        companionStatus.push_back(QVariantMap{{"layer", SourceManagedWindowLayer(window)},
            {"x", window->m_x}, {"y", window->m_y}, {"width", window->m_w}, {"height", window->m_h},
            {"texts", texts}, {"gauges", gauges}, {"buttons", buttons}, {"nameEdit", nameEdit}});
    }
    chatUi.insert("companionStatus", companionStatus);
    QVariantMap instance;
    if (const auto* window = g_windowMgr.m_instanceWnd; window && IsGameplayWindowVisible(state, window)) {
        const auto display = window->GetDisplay(static_cast<u64>(std::time(nullptr)));
        QVariantList details;
        for (const auto& row : display.details) details.push_back(ToQString(row));
        instance = {{"layer", SourceManagedWindowLayer(window)}, {"x", window->m_x}, {"y", window->m_y},
            {"width", window->m_w}, {"height", window->m_h}, {"title", ToQString(display.title)},
            {"status", ToQString(display.status)}, {"details", details}};
    }
    chatUi.insert("instance", instance);
    QVariantMap questJournal;
    if (const auto* window = g_windowMgr.m_questWnd; window && IsGameplayWindowVisible(state, window)) {
        const auto display = window->GetDisplay();
        QVariantList rows;
        for (const auto& row : display.rows) {
            rows.push_back(QVariantMap{{"title", ToQString(row.title)}, {"active", row.active},
                {"selected", row.selected}});
        }
        QVariantList objectives;
        for (const auto& row : display.objectives) objectives.push_back(ToQString(row));
        questJournal = {{"layer", SourceManagedWindowLayer(window)}, {"x", window->m_x}, {"y", window->m_y},
            {"width", window->m_w}, {"height", window->m_h}, {"rows", rows}, {"title", ToQString(display.title)},
            {"description", ToQString(display.description)}, {"expiry", ToQString(display.expiry)},
            {"objectives", objectives}, {"action", ToQString(display.action)},
            {"previous", display.previous}, {"next", display.next}};
    }
    chatUi.insert("questJournal", questJournal);
    QVariantList companionSkillLists;
    for (auto* window : {g_windowMgr.m_homunSkillListWnd, g_windowMgr.m_merSkillListWnd}) {
        if (!window || !IsGameplayWindowVisible(state, window)) continue;
        QVariantList rows;
        for (const auto& row : window->GetRows()) {
            rows.push_back(QVariantMap{{"y", row.y}, {"name", ToQString(row.name)}, {"level", row.level},
                {"useLevel", row.useLevel}, {"levelSelectable", row.levelSelectable},
                {"spCost", row.spCost}, {"selected", row.selected}, {"icon", ToQString(row.icon->uri)},
                {"upgradeVisible", row.upgrade != nullptr}, {"upgradeX", row.upgradeX},
                {"upgradeY", row.upgradeY}, {"upgradeSize", row.upgradeSize},
                {"upgradeArt", row.upgrade ? ToQString(row.upgrade->uri) : QString()}});
        }
        const auto& close = window->GetCloseButton();
        const RECT scrollThumb = window->GetScrollThumb();
        const auto& base = window->GetBaseButton();
        const auto& mini = window->GetMiniButton();
        QVariantList skin;
        for (const auto& piece : window->GetSkinPieces()) {
            skin.push_back(QVariantMap{{"x", static_cast<int>(piece.bounds.left)},
                {"y", static_cast<int>(piece.bounds.top)}, {"width", static_cast<int>(piece.bounds.right - piece.bounds.left)},
                {"height", static_cast<int>(piece.bounds.bottom - piece.bounds.top)},
                {"control", piece.control}, {"art", ToQString(piece.art->uri)}});
        }
        companionSkillLists.push_back(QVariantMap{{"layer", SourceManagedWindowLayer(window)},
            {"x", window->m_x}, {"y", window->m_y}, {"width", window->m_w}, {"height", window->m_h},
            {"title", ToQString(window->GetTitle())}, {"footer", ToQString(window->GetFooter())},
            {"close", ToQString(close.GetButton().uris[close.GetVisualState()])}, {"rows", rows},
            {"scrollX", static_cast<int>(scrollThumb.left)}, {"scrollY", static_cast<int>(scrollThumb.top)},
            {"scrollHeight", static_cast<int>(scrollThumb.bottom - scrollThumb.top)}, {"skin", skin},
            {"base", ToQString(base.GetButton().uris[base.GetVisualState()])},
            {"mini", ToQString(mini.GetButton().uris[mini.GetVisualState()])}});
    }
    chatUi.insert("companionSkillLists", companionSkillLists);
    const auto roomButton=[](const RoomSettingsButton& button,int status){
        const auto bounds=button.Bounds();const auto& bitmap=button.bitmaps[status];
        return QVariantMap{{"id",button.id},{"x",static_cast<int>(bounds.left)},{"y",static_cast<int>(bounds.top)},
            {"width",static_cast<int>(bounds.right-bounds.left)},{"height",static_cast<int>(bounds.bottom-bounds.top)},
            {"imageWidth",bitmap.width},{"imageHeight",bitmap.height},{"artwork",QString::fromStdString(button.uris[status])},
            {"pressed",status==2},{"label",button.id==118 || button.id==121?"OK":button.id==119 || button.id==122?"Cancel":button.id==135?"x":"v"}};
    };
    QVariantMap petEgg;
    if(auto* window=g_windowMgr.m_petEggListWnd){
        QVariantList rows,pieces,buttons;
        for(const auto& row:window->GetVisibleRows())rows.push_back(QVariantMap{
            {"index",row.index},{"itemIndex",row.itemIndex},{"itemId",row.itemId},{"y",row.y},
            {"selected",row.selected},{"text",QString::fromUtf8(row.text.c_str())},
            {"artwork",QString::fromStdString(row.artwork)},{"imageWidth",row.bitmap.width},{"imageHeight",row.bitmap.height}});
        for(const auto& piece:window->GetFramePieces())pieces.push_back(QVariantMap{
            {"x",piece.x},{"y",piece.y},{"width",piece.bitmap.width},{"height",piece.bitmap.height},
            {"phase",piece.phase},{"artwork",QString::fromStdString(piece.uri)}});
        for(const auto* button:window->GetButtons()){
            auto data=roomButton(button->GetButton(),button->GetVisualState());
            data["x"]=button->m_x-window->m_x;data["y"]=button->m_y-window->m_y;
            data["width"]=button->m_w;data["height"]=button->m_h;data["pressed"]=button->IsLogicallyPressed();
            buttons.push_back(data);
        }
        const auto rect=[](RECT r){return QVariantMap{{"x",static_cast<int>(r.left)},{"y",static_cast<int>(r.top)},
            {"width",static_cast<int>(r.right-r.left)},{"height",static_cast<int>(r.bottom-r.top)}};};
        const RECT track{185,34,198,179};const auto layout=ro::chat::ScrollLayout(track,window->GetScrollBar());
        petEgg={{"visible",IsGameplayWindowVisible(state,window)},{"x",window->m_x},{"y",window->m_y},
            {"width",window->m_w},{"height",window->m_h},{"layer",SourceManagedWindowLayer(window)},
            {"title",QString::fromUtf8(window->GetTitle().c_str())},{"current",window->GetCurrentItem()},{"offset",window->GetOffset()},
            {"rows",rows},{"framePieces",pieces},{"buttons",buttons},
            {"scrollTrack",rect(track)},{"scrollThumb",rect(layout.thumb)},{"scrollUp",rect(layout.up)},{"scrollDown",rect(layout.down)}};
    }
    chatUi.insert("petEgg",petEgg);
    QVariantMap petTaming;
    if(const auto* window=g_windowMgr.m_petTamingDeceiveWnd){
        petTaming={{"visible",IsGameplayWindowVisible(state,window)},{"x",window->m_x},{"y",window->m_y},
            {"width",window->m_w},{"height",window->m_h},{"layer",SourceManagedWindowLayer(window)},
            {"artwork",QString::fromStdString(window->GetArtworkUri())},{"targetGid",window->GetTargetGid()},
            {"selected",window->IsSelected()},{"action",window->GetAction()},{"delay",window->GetDelay()},
            {"destination",window->GetDestination()}};
    }
    chatUi.insert("petTaming",petTaming);
    QVariantList settings;
    for(const auto* window:{g_windowMgr.m_chatRoomMakeWnd,g_windowMgr.m_chatRoomChangeWnd}){
        if(!window)continue;QVariantList buttons,popupRows;
        for(const auto& button:window->GetButtons())buttons.push_back(roomButton(button,window->GetPressedButton()==button.id?2:window->GetHoveredButton()==button.id?1:0));
        const int popup=window->GetPopupKind();const auto bounds=window->GetPopupBounds();
        if(popup>=0)for(int i=0;i<(popup?1:3);++i)popupRows.push_back(ToQString(popup?window->GetClassLabel():window->GetPeopleLabel(window->GetPopupFirst()+i)));
        settings.push_back(QVariantMap{{"visible",IsGameplayWindowVisible(state,window)},{"layer",chatLayer(window)},{"change",window->IsChange()},
            {"x",window->m_x},{"y",window->m_y},{"width",window->m_w},{"height",window->m_h},{"caption",ToQString(window->GetCaption())},
            {"title",ToQString(window->GetTitleText())},{"masked",QString(static_cast<int>(window->GetPasswordText().size()),QChar('*'))},
            {"private",window->IsPrivate()},{"publicLabel",ToQString(window->GetPrivacyLabel(false))},{"privateLabel",ToQString(window->GetPrivacyLabel(true))},
            {"people",window->GetPeople()},{"peopleLabel",ToQString(window->GetPeopleLabel())},{"classLabel",ToQString(window->GetClassLabel())},
            {"titlebar",QString::fromStdString(window->GetFrameUri(true))},{"background",QString::fromStdString(window->GetFrameUri(false))},
            {"buttons",buttons},{"popupVisible",popup>=0},{"popupX",static_cast<int>(bounds.left)},{"popupY",static_cast<int>(bounds.top)},
            {"popupWidth",static_cast<int>(bounds.right-bounds.left)},{"popupHeight",static_cast<int>(bounds.bottom-bounds.top)},{"popupRows",popupRows}});
    }
    chatUi.insert("roomSettings",settings);
    QVariantMap message;
    if(const auto* window=g_windowMgr.m_roomMessageBox){
        message={{"visible",IsGameplayWindowVisible(state,window)},{"layer",chatLayer(window)},
            {"x",window->m_x},{"y",window->m_y},{"width",window->m_w},{"height",window->m_h},
            {"text",ToQString(window->GetText())},{"messageId",window->GetMessageId()},
            {"bodyHeight",window->GetMessageBodyHeight()},
            {"hasCancel",window->HasCancel()},{"button",roomButton(window->GetButton(),window->IsPressed()?2:0)},
            {"cancelButton",roomButton(window->GetCancelButton(),window->IsCancelPressed()?2:0)}};
    }
    chatUi.insert("roomMessage",message);
    QVariantList roomTitles;
    for(const auto* child:g_windowMgr.m_children){
        if(const auto* title=dynamic_cast<const UIChatRoomTitle*>(child)){
            roomTitles.push_back(QVariantMap{{"visible",IsGameplayWindowVisible(state,title)},{"layer",chatLayer(title)},
                {"x",title->m_x},{"y",title->m_y},{"width",title->m_w},{"height",title->m_h},
                {"id",static_cast<qulonglong>(title->m_roomInfo.roomId)},{"text",ToQString(title->GetDisplayTitle())},
                {"fullText",ToQString(title->GetTitle())},{"artwork",QString::fromStdString(title->GetArtworkUri())}});
        }
    }
    chatUi.insert("roomTitles",roomTitles);
    QVariantMap password;
    if(const auto* dialog=g_windowMgr.m_chatRoomPasswordWnd){
        const auto ok=dialog->GetOkBounds();
        password={{"visible",IsGameplayWindowVisible(state,dialog)},{"layer",chatLayer(dialog)},
            {"x",dialog->m_x},{"y",dialog->m_y},{"width",dialog->m_w},{"height",dialog->m_h},
            {"id",static_cast<qulonglong>(dialog->GetRoomId())},{"prompt",ToQString(dialog->GetPrompt())},
            {"masked",QString(static_cast<int>(dialog->GetInputText().size()),QChar('*'))},{"pressedOk",dialog->IsOkPressed()},{"okX",static_cast<int>(ok.left)},{"okY",static_cast<int>(ok.top)},
            {"okWidth",static_cast<int>(ok.right-ok.left)},{"okHeight",static_cast<int>(ok.bottom-ok.top)},
            {"okArtwork",QString::fromStdString(dialog->GetOkArtworkUri())},{"okImageWidth",dialog->GetOkArtworkWidth()},{"okImageHeight",dialog->GetOkArtworkHeight()}};
    }
    chatUi.insert("roomPassword",password);
    QVariantMap room;
    if(const auto* window=g_windowMgr.m_chatRoomWnd){
        room.insert("visible",IsGameplayWindowVisible(state,window));room.insert("layer",chatLayer(window));
        room.insert("x",window->m_x);room.insert("y",window->m_y);room.insert("width",window->m_w);room.insert("height",window->m_h);
        room.insert("title",ToQString(window->GetTitle()));room.insert("input",ToQString(window->GetInputText()));
        room.insert("id",static_cast<qulonglong>(window->GetInfo().roomId));
        const auto rect=[](RECT r){return QVariantMap{{"x",static_cast<int>(r.left)},{"y",static_cast<int>(r.top)},
            {"width",static_cast<int>(r.right-r.left)},{"height",static_cast<int>(r.bottom-r.top)}};};
        const auto layout=window->GetLayout();
        room.insert("history",rect(layout.history));room.insert("members",rect(layout.members));room.insert("edit",rect(layout.edit));
        room.insert("close",rect(layout.close));room.insert("resizer",rect(layout.resizer));
        room.insert("scrollTrack",rect(layout.scrollTrack));room.insert("scrollThumb",rect(layout.scrollThumb));
        room.insert("scrollUp",rect(layout.scrollUp));room.insert("scrollDown",rect(layout.scrollDown));
        room.insert("memberTrack",rect(layout.memberTrack));room.insert("memberThumb",rect(layout.memberThumb));
        room.insert("memberUp",rect(layout.memberUp));room.insert("memberDown",rect(layout.memberDown));
        room.insert("scrollVisible",window->GetScrollModel().maximum>0);room.insert("memberScrollVisible",window->GetScrollModel(true).maximum>0);
        QVariantList rows;
        for(const auto& row:window->GetVisibleRows())rows.push_back(QVariantMap{{"text",ToQString(row.text)},{"color",QColor::fromRgb(row.color).name()}});
        room.insert("rows",rows);rows.clear();
        const auto& members=window->GetMemberRows();const int first=window->GetMemberFirst();
        for(int i=first;i<static_cast<int>(members.size()) && i<first+window->GetScrollModel(true).page;++i){
            if(i<0)continue;
            rows.push_back(QVariantMap{{"name",ToQString(members[i].name)},{"role",static_cast<qulonglong>(members[i].role)},
                {"selected",i==window->GetSelectedMember()},{"color",members[i].role?"#000000":"#008080"}});
        }
        room.insert("memberRows",rows);
    }
    chatUi.insert("room",room);
    QVariantList histories;
    for (const auto* history : {g_windowMgr.m_subChatWndST, g_windowMgr.m_subChatWndBT}) {
        if (!history) continue;
        QVariantMap value;
        value.insert(QStringLiteral("kind"), history->GetKind());
        value.insert(QStringLiteral("label"), ToQString(history->GetSourceLabel()));
        value.insert(QStringLiteral("layer"), chatLayer(history));
        value.insert(QStringLiteral("visible"), IsGameplayWindowVisible(state, history));
        value.insert(QStringLiteral("x"), history->m_x);
        value.insert(QStringLiteral("y"), history->m_y);
        value.insert(QStringLiteral("width"), history->m_w);
        value.insert(QStringLiteral("height"), history->m_h);
        const auto layout=history->GetHistoryLayout();
        const auto rectMap=[](RECT rect) { return QVariantMap{{"x",static_cast<int>(rect.left)},{"y",static_cast<int>(rect.top)},
            {"width",static_cast<int>(rect.right-rect.left)},{"height",static_cast<int>(rect.bottom-rect.top)}}; };
        value.insert(QStringLiteral("history"),rectMap(layout.history));
        value.insert(QStringLiteral("battleOption"),rectMap(history->GetBattleOptionButton()));
        value.insert(QStringLiteral("scrollTrack"),rectMap(layout.scrollTrack));
        value.insert(QStringLiteral("scrollThumb"),rectMap(layout.scrollThumb));
        value.insert("scrollUp",rectMap(layout.scrollUp));value.insert("scrollDown",rectMap(layout.scrollDown));
        value.insert(QStringLiteral("first"),layout.first);
        value.insert(QStringLiteral("total"),layout.total);
        value.insert(QStringLiteral("scrollVisible"),layout.total>layout.visible);
        QVariantList rows;
        for (const auto& row : history->GetVisibleRows()) {
            QVariantMap entry;
            entry.insert(QStringLiteral("text"), ToQString(row.text));
            entry.insert(QStringLiteral("color"), QColor::fromRgb(row.color).name());
            entry.insert(QStringLiteral("talker"), ToQString(row.talker));
            rows.push_back(entry);
        }
        value.insert(QStringLiteral("rows"), rows);
        histories.push_back(value);
    }
    chatUi.insert(QStringLiteral("histories"), histories);
    QVariantMap mini;
    if (const auto* window = g_windowMgr.m_subChatMiniWnd) {
        mini.insert(QStringLiteral("visible"), IsGameplayWindowVisible(state, window));
        mini.insert(QStringLiteral("layer"), chatLayer(window));
        mini.insert(QStringLiteral("x"), window->m_x);
        mini.insert(QStringLiteral("y"), window->m_y);
        mini.insert(QStringLiteral("st"), g_windowMgr.m_onStSubChat);
        mini.insert(QStringLiteral("bt"), g_windowMgr.m_onBtSubChat);
        mini.insert(QStringLiteral("stLabel"), ToQString(window->GetSourceLabel(0)));
        mini.insert(QStringLiteral("btLabel"), ToQString(window->GetSourceLabel(1)));
    }
    chatUi.insert(QStringLiteral("mini"), mini);
    QVariantMap options;
    if(const auto* window=g_windowMgr.m_battleMsgOptionWnd){
        const auto layout=window->GetLayout();
        const auto rectMap=[](RECT r){return QVariantMap{{"x",static_cast<int>(r.left)},{"y",static_cast<int>(r.top)},
            {"width",static_cast<int>(r.right-r.left)},{"height",static_cast<int>(r.bottom-r.top)}};};
        options={{"visible",IsGameplayWindowVisible(state,window)},{"layer",chatLayer(window)},
            {"x",window->m_x},{"y",window->m_y},{"width",window->m_w},{"height",window->m_h},
            {"title",ToQString(window->GetTitle())},{"first",layout.first},
            {"base",rectMap(layout.base)},{"close",rectMap(layout.close)},
            {"scrollTrack",rectMap(layout.scrollTrack)},{"scrollThumb",rectMap(layout.scrollThumb)},
            {"scrollUp",rectMap(layout.scrollUp)},{"scrollDown",rectMap(layout.scrollDown)}};
        QVariantList rows;
        for(int i=0;i<layout.rows && layout.first+i<13;++i){
            const int index=layout.first+i;
            rows.push_back(QVariantMap{{"index",index},{"text",ToQString(window->GetLabel(index))},
                {"enabled",g_session.m_battleMsgOnOffSet[index]},{"selected",layout.selected==index}});
        }
        options.insert("rows",rows);
    }
    chatUi.insert("battleOptions",options);
}

void PopulateChatWindowState(QtUiState* state)
{
    if (!state) {
        return;
    }
    if(auto* editor=g_windowMgr.m_autoMessageWnd)if(IsGameplayWindowVisible(state,editor))editor->FocusForDraw();

    const UINewChatWnd* const chatWnd = g_windowMgr.m_chatWnd;
    const bool visible = IsGameplayWindowVisible(state, chatWnd);
    state->setChatWindowVisible(visible);
    if (!visible) {
        state->setChatWindowGeometry(0, 0, 0, 0);
        state->setChatWindowInputActive(false);
        state->setChatWindowWhisperInputActive(false);
        state->setChatWindowMessageInputActive(false);
        state->setChatWindowWhisperTargetText(QString());
        state->setChatWindowInputText(QString());
        state->setChatWindowLines(QVariantList{});
        state->setChatWindowScrollBar(QVariantMap{});
        QVariantMap auxiliary;
        PopulateChatAuxiliaryState(state, auxiliary);
        state->setChatWindowUi(auxiliary);
        return;
    }

    state->setChatWindowGeometry(chatWnd->m_x, chatWnd->m_y, chatWnd->m_w, chatWnd->m_h);
    state->setChatWindowInputActive(chatWnd->IsInputActive());
    state->setChatWindowWhisperInputActive(chatWnd->IsWhisperTargetActive());
    state->setChatWindowMessageInputActive(chatWnd->IsMessageInputActive());
    state->setChatWindowWhisperTargetText(ToQString(chatWnd->GetWhisperTargetText()));
    state->setChatWindowInputText(ToQString(chatWnd->GetInputText()));

    QVariantList lines;
    const std::vector<ChatLine> visibleLines = chatWnd->GetVisibleLines();
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

    const ChatScrollBarState scrollState = chatWnd->GetScrollBarState();
    QVariantMap scrollBar;
    scrollBar.insert(QStringLiteral("visible"), scrollState.visible != 0);
    scrollBar.insert(QStringLiteral("totalLines"), scrollState.totalLines);
    scrollBar.insert(QStringLiteral("firstVisibleLine"), scrollState.firstVisibleLine);
    scrollBar.insert(QStringLiteral("visibleLineCount"), scrollState.visibleLineCount);
    state->setChatWindowScrollBar(scrollBar);

    QVariantMap chatUi;
    PopulateChatAuxiliaryState(state, chatUi);
    const auto layout = chatWnd->GetLayoutDisplay();
    const auto rectMap = [](const RECT& rect) {
        QVariantMap value;
        value.insert(QStringLiteral("x"), static_cast<int>(rect.left));
        value.insert(QStringLiteral("y"), static_cast<int>(rect.top));
        value.insert(QStringLiteral("width"), static_cast<int>(rect.right - rect.left));
        value.insert(QStringLiteral("height"), static_cast<int>(rect.bottom - rect.top));
        return value;
    };
    QVariantMap sourceRects;
    sourceRects.insert(QStringLiteral("header"), rectMap(layout.header));
    sourceRects.insert(QStringLiteral("history"), rectMap(layout.history));
    sourceRects.insert(QStringLiteral("scrollTrack"), rectMap(layout.scrollTrack));
    sourceRects.insert(QStringLiteral("scrollThumb"), rectMap(layout.scrollThumb));
    sourceRects.insert("scrollUp",rectMap(layout.scrollUp));sourceRects.insert("scrollDown",rectMap(layout.scrollDown));
    sourceRects.insert(QStringLiteral("whisper"), rectMap(layout.whisperInput));
    sourceRects.insert(QStringLiteral("message"), rectMap(layout.messageInput));
    sourceRects.insert(QStringLiteral("gear"), rectMap(layout.gearButton));
    sourceRects.insert(QStringLiteral("battleOption"),rectMap(layout.battleOptionButton));
    sourceRects.insert(QStringLiteral("stick"), rectMap(layout.stickButton));
    chatUi.insert(QStringLiteral("sourceRects"), sourceRects);

    chatUi.insert(QStringLiteral("expansionState"), chatWnd->GetExpansionState());
    chatUi.insert(QStringLiteral("expanded"), chatWnd->GetExpansionState() != 0);
    QVariantList tabs;
    tabs.reserve(chatWnd->GetTabCount());
    for (int index = 0; index < chatWnd->GetTabCount(); ++index) {
        ChatTabDisplay tab{};
        if (!chatWnd->GetTabDisplay(index, &tab)) {
            continue;
        }

        QVariantMap entry;
        entry.insert(QStringLiteral("id"), tab.id);
        entry.insert(QStringLiteral("label"), ToQString(tab.label));
        entry.insert(QStringLiteral("active"), tab.active != 0);
        const auto bounds = rectMap(layout.tabs[index]);
        for (auto field = bounds.cbegin(); field != bounds.cend(); ++field) entry.insert(field.key(), field.value());
        tabs.push_back(entry);
    }
    chatUi.insert(QStringLiteral("tabs"), tabs);
    chatUi.insert(QStringLiteral("fontPixelSize"), chatWnd->GetFontPixelSize());
    chatUi.insert(QStringLiteral("windowOpacityPercent"), chatWnd->GetWindowOpacityPercent());
    chatUi.insert(QStringLiteral("idleOpacityPercent"), chatWnd->GetIdleOpacityPercent());
    chatUi.insert(QStringLiteral("focused"), chatWnd->IsWindowFocused());
    chatUi.insert(QStringLiteral("configVisible"), chatWnd->IsConfigVisible());

    int configX = 0;
    int configY = 0;
    int configWidth = 0;
    int configHeight = 0;
    if (chatWnd->GetConfigRectForQt(&configX, &configY, &configWidth, &configHeight)) {
        chatUi.insert(QStringLiteral("configX"), configX);
        chatUi.insert(QStringLiteral("configY"), configY);
        chatUi.insert(QStringLiteral("configWidth"), configWidth);
        chatUi.insert(QStringLiteral("configHeight"), configHeight);
    }

    QVariantList filters;
    filters.reserve(chatWnd->GetFilterOptionCount());
    for (int index = 0; index < chatWnd->GetFilterOptionCount(); ++index) {
        ChatFilterOptionDisplay filter{};
        if (!chatWnd->GetFilterOptionDisplay(index, &filter)) {
            continue;
        }

        QVariantMap entry;
        entry.insert(QStringLiteral("id"), filter.id);
        entry.insert(QStringLiteral("label"), ToQString(filter.label));
        entry.insert(QStringLiteral("enabled"), filter.enabled != 0);
        filters.push_back(entry);
    }
    chatUi.insert(QStringLiteral("filters"), filters);
    state->setChatWindowUi(chatUi);
}

void PopulateRechargeGaugeState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIRechargeGage* rechargeGauge = nullptr;
    if (const auto* mode = g_modeMgr.GetCurrentGameMode()) {
        const auto* gauge = mode->GetNpcProgressGauge();
        if (gauge && gauge->m_show != 0) {
            rechargeGauge = gauge;
        }
    }
    if (!rechargeGauge) {
        for (UIWindow* const window : g_windowMgr.m_children) {
            UIRechargeGage* const gauge = dynamic_cast<UIRechargeGage*>(window);
            if (gauge && gauge->m_show != 0) {
                rechargeGauge = gauge;
                break;
            }
        }
    }

    const bool visible = rechargeGauge != nullptr && !IsGameplayUiSuppressed(state);
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
    const bool visible = IsGameplayWindowVisible(state, itemWnd);
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

void PopulateStorageState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIStorageWnd* const storageWnd = g_windowMgr.m_storageWnd;
    const bool visible = IsGameplayWindowVisible(state, storageWnd);
    state->setStorageVisible(visible);
    if (!visible) {
        state->setStorageGeometry(0, 0, 0, 0);
        state->setStorageMini(false);
        state->setStorageTab(0);
        state->setStorageData(QVariantMap{});
        return;
    }

    state->setStorageGeometry(storageWnd->m_x, storageWnd->m_y, storageWnd->m_w, storageWnd->m_h);
    state->setStorageMini(storageWnd->IsMiniMode());

    UIStorageWnd::DisplayData display{};
    QVariantMap data;
    if (storageWnd->GetDisplayDataForQt(&display)) {
        state->setStorageTab(display.currentTab);
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
        systemButtons.reserve(storageWnd->GetQtSystemButtonCount());
        for (int index = 0; index < storageWnd->GetQtSystemButtonCount(); ++index) {
            UIStorageWnd::QtButtonDisplay buttonDisplay{};
            if (!storageWnd->GetQtSystemButtonDisplayForQt(index, &buttonDisplay)) {
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
        tabs.reserve(storageWnd->GetQtTabCount());
        for (int index = 0; index < storageWnd->GetQtTabCount(); ++index) {
            UIStorageWnd::QtButtonDisplay tabDisplay{};
            if (!storageWnd->GetQtTabDisplayForQt(index, &tabDisplay)) {
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
        for (const UIStorageWnd::DisplaySlot& slot : display.displaySlots) {
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
        state->setStorageTab(0);
    }
    state->setStorageData(data);
}

void PopulateFriendPartyState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIMessengerGroupWnd* const friendPartyWnd = g_windowMgr.m_messengerGroupWnd;
    const bool visible = IsGameplayWindowVisible(state, friendPartyWnd);
    state->setFriendPartyVisible(visible);
    if (!visible) {
        state->setFriendPartyGeometry(0, 0, 0, 0);
        state->setFriendPartyData(QVariantMap{});
        return;
    }

    state->setFriendPartyGeometry(friendPartyWnd->m_x, friendPartyWnd->m_y, friendPartyWnd->m_w, friendPartyWnd->m_h);

    UIMessengerGroupWnd::DisplayData display{};
    QVariantMap data;
    if (friendPartyWnd->GetDisplayDataForQt(&display)) {
        data.insert(QStringLiteral("title"), ToQString(display.title));
        data.insert(QStringLiteral("summaryTitle"), ToQString(display.summaryTitle));
        data.insert(QStringLiteral("summaryValue"), ToQString(display.summaryValue));
        data.insert(QStringLiteral("summarySecondaryValue"), ToQString(display.summarySecondaryValue));
        data.insert(QStringLiteral("currentTab"), display.currentTab);
        data.insert(QStringLiteral("selectedIndex"), display.selectedIndex);
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
        systemButtons.reserve(friendPartyWnd->GetQtSystemButtonCount());
        for (int index = 0; index < friendPartyWnd->GetQtSystemButtonCount(); ++index) {
            UIMessengerGroupWnd::QtButtonDisplay buttonDisplay{};
            if (!friendPartyWnd->GetQtSystemButtonDisplayForQt(index, &buttonDisplay)) {
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
        tabs.reserve(friendPartyWnd->GetQtTabCount());
        for (int index = 0; index < friendPartyWnd->GetQtTabCount(); ++index) {
            UIMessengerGroupWnd::QtButtonDisplay tabDisplay{};
            if (!friendPartyWnd->GetQtTabDisplayForQt(index, &tabDisplay)) {
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

        QVariantList rows;
        rows.reserve(static_cast<qsizetype>(display.rows.size()));
        for (const UIMessengerGroupWnd::DisplayRow& row : display.rows) {
            QVariantMap entry;
            entry.insert(QStringLiteral("x"), row.x);
            entry.insert(QStringLiteral("y"), row.y);
            entry.insert(QStringLiteral("width"), row.width);
            entry.insert(QStringLiteral("height"), row.height);
            entry.insert(QStringLiteral("selected"), row.selected);
            entry.insert(QStringLiteral("color"), row.color);
            entry.insert(QStringLiteral("name"), ToQString(row.name));
            entry.insert(QStringLiteral("status"), ToQString(row.status));
            entry.insert(QStringLiteral("detail"), ToQString(row.detail));
            rows.push_back(entry);
        }
        data.insert(QStringLiteral("rows"), rows);

        QVariantList actionButtons;
        actionButtons.reserve(static_cast<qsizetype>(display.actionButtons.size()));
        for (const UIMessengerGroupWnd::QtButtonDisplay& buttonDisplay : display.actionButtons) {
            QVariantMap button;
            button.insert(QStringLiteral("id"), buttonDisplay.id);
            button.insert(QStringLiteral("x"), buttonDisplay.x);
            button.insert(QStringLiteral("y"), buttonDisplay.y);
            button.insert(QStringLiteral("width"), buttonDisplay.width);
            button.insert(QStringLiteral("height"), buttonDisplay.height);
            button.insert(QStringLiteral("label"), ToQString(buttonDisplay.label));
            button.insert(QStringLiteral("visible"), buttonDisplay.visible);
            button.insert(QStringLiteral("active"), buttonDisplay.active);
            actionButtons.push_back(button);
        }
        data.insert(QStringLiteral("actionButtons"), actionButtons);
        data.insert("layer",SourceManagedWindowLayer(friendPartyWnd));
        UIMessengerGroupWnd::QtButtonDisplay sourceButton{};
        if(friendPartyWnd->GetQtFriendOptionButtonDisplay(&sourceButton))data.insert("sourceOptions",QVariantMap{
            {"id",sourceButton.id},{"x",sourceButton.x},{"y",sourceButton.y},{"width",sourceButton.width},{"height",sourceButton.height},{"visible",sourceButton.visible},
            {"label",ToQString(sourceButton.label)},{"artwork",QString::fromStdString(sourceButton.artwork)},{"imageWidth",sourceButton.imageWidth},{"imageHeight",sourceButton.imageHeight},{"state",sourceButton.state}});
    }

    state->setFriendPartyData(data);
}

void PopulatePartySetupState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIPartyOptionWnd* const partySetupWnd = g_windowMgr.m_partyOptionWnd;
    const bool visible = IsGameplayWindowVisible(state, partySetupWnd);
    state->setPartySetupVisible(visible);
    if (!visible) {
        state->setPartySetupGeometry(0, 0, 0, 0);
        state->setPartySetupData(QVariantMap{});
        return;
    }

    state->setPartySetupGeometry(partySetupWnd->m_x, partySetupWnd->m_y, partySetupWnd->m_w, partySetupWnd->m_h);

    UIPartyOptionWnd::DisplayData display{};
    QVariantMap data;
    if (partySetupWnd->GetDisplayDataForQt(&display)) {
        data.insert(QStringLiteral("title"), ToQString(display.title));
        data.insert(QStringLiteral("showNameEdit"), display.showNameEdit);
        data.insert(QStringLiteral("nameFocused"), display.nameFocused);
        data.insert(QStringLiteral("nameLabel"), ToQString(display.nameLabel));
        data.insert(QStringLiteral("nameValue"), ToQString(display.nameValue));
        data.insert(QStringLiteral("nameFieldX"), display.nameFieldX);
        data.insert(QStringLiteral("nameFieldY"), display.nameFieldY);
        data.insert(QStringLiteral("nameFieldWidth"), display.nameFieldWidth);
        data.insert(QStringLiteral("nameFieldHeight"), display.nameFieldHeight);
        data.insert(QStringLiteral("tooltipText"), ToQString(display.tooltipText));
        data.insert(QStringLiteral("tooltipX"), display.tooltipX);
        data.insert(QStringLiteral("tooltipY"), display.tooltipY);

        QVariantList labels;
        labels.reserve(static_cast<qsizetype>(display.labels.size()));
        for (const UIPartyOptionWnd::DisplayLabel& label : display.labels) {
            QVariantMap entry;
            entry.insert(QStringLiteral("x"), label.x);
            entry.insert(QStringLiteral("y"), label.y);
            entry.insert(QStringLiteral("width"), label.width);
            entry.insert(QStringLiteral("height"), label.height);
            entry.insert(QStringLiteral("text"), ToQString(label.text));
            entry.insert(QStringLiteral("header"), label.header);
            labels.push_back(entry);
        }
        data.insert(QStringLiteral("labels"), labels);

        QVariantList choices;
        choices.reserve(static_cast<qsizetype>(display.choices.size()));
        for (const UIPartyOptionWnd::DisplayChoice& choice : display.choices) {
            QVariantMap entry;
            entry.insert(QStringLiteral("groupId"), choice.groupId);
            entry.insert(QStringLiteral("optionId"), choice.optionId);
            entry.insert(QStringLiteral("x"), choice.x);
            entry.insert(QStringLiteral("y"), choice.y);
            entry.insert(QStringLiteral("width"), choice.width);
            entry.insert(QStringLiteral("height"), choice.height);
            entry.insert(QStringLiteral("label"), ToQString(choice.label));
            entry.insert(QStringLiteral("selected"), choice.selected);
            entry.insert(QStringLiteral("enabled"), choice.enabled);
            choices.push_back(entry);
        }
        data.insert(QStringLiteral("choices"), choices);

        QVariantList helpIcons;
        helpIcons.reserve(static_cast<qsizetype>(display.helpIcons.size()));
        for (const UIPartyOptionWnd::DisplayHelpIcon& helpIcon : display.helpIcons) {
            QVariantMap entry;
            entry.insert(QStringLiteral("groupId"), helpIcon.groupId);
            entry.insert(QStringLiteral("x"), helpIcon.x);
            entry.insert(QStringLiteral("y"), helpIcon.y);
            entry.insert(QStringLiteral("width"), helpIcon.width);
            entry.insert(QStringLiteral("height"), helpIcon.height);
            entry.insert(QStringLiteral("hovered"), helpIcon.hovered);
            helpIcons.push_back(entry);
        }
        data.insert(QStringLiteral("helpIcons"), helpIcons);

        QVariantList buttons;
        buttons.reserve(static_cast<qsizetype>(display.buttons.size()));
        for (const UIPartyOptionWnd::QtButtonDisplay& buttonDisplay : display.buttons) {
            QVariantMap button;
            button.insert(QStringLiteral("id"), buttonDisplay.id);
            button.insert(QStringLiteral("x"), buttonDisplay.x);
            button.insert(QStringLiteral("y"), buttonDisplay.y);
            button.insert(QStringLiteral("width"), buttonDisplay.width);
            button.insert(QStringLiteral("height"), buttonDisplay.height);
            button.insert(QStringLiteral("label"), ToQString(buttonDisplay.label));
            button.insert(QStringLiteral("visible"), buttonDisplay.visible);
            button.insert(QStringLiteral("active"), buttonDisplay.active);
            buttons.push_back(button);
        }
        data.insert(QStringLiteral("buttons"), buttons);
    }

    state->setPartySetupData(data);
}

void PopulatePartyInviteState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIJoinPartyAcceptWnd* const partyInviteWnd = g_windowMgr.m_joinPartyAcceptWnd;
    const bool visible = IsGameplayWindowVisible(state, partyInviteWnd);
    state->setPartyInviteVisible(visible);
    if (!visible) {
        state->setPartyInviteGeometry(0, 0, 0, 0);
        state->setPartyInviteData(QVariantMap{});
        return;
    }

    state->setPartyInviteGeometry(partyInviteWnd->m_x, partyInviteWnd->m_y, partyInviteWnd->m_w, partyInviteWnd->m_h);

    UIJoinPartyAcceptWnd::DisplayData display{};
    QVariantMap data;
    if (partyInviteWnd->GetDisplayDataForQt(&display)) {
        data.insert(QStringLiteral("title"), ToQString(display.title));
        data.insert(QStringLiteral("bodyText"), ToQString(display.bodyText));

        QVariantList messageLines;
        messageLines.reserve(static_cast<qsizetype>(display.messageLines.size()));
        for (const std::string& line : display.messageLines) {
            messageLines.push_back(ToQString(line));
        }
        data.insert(QStringLiteral("messageLines"), messageLines);

        QVariantList buttons;
        buttons.reserve(static_cast<qsizetype>(display.buttons.size()));
        for (const UIJoinPartyAcceptWnd::QtButtonDisplay& buttonDisplay : display.buttons) {
            QVariantMap button;
            button.insert(QStringLiteral("id"), buttonDisplay.id);
            button.insert(QStringLiteral("x"), buttonDisplay.x);
            button.insert(QStringLiteral("y"), buttonDisplay.y);
            button.insert(QStringLiteral("width"), buttonDisplay.width);
            button.insert(QStringLiteral("height"), buttonDisplay.height);
            button.insert(QStringLiteral("label"), ToQString(buttonDisplay.label));
            button.insert(QStringLiteral("active"), buttonDisplay.active);
            buttons.push_back(button);
        }
        data.insert(QStringLiteral("buttons"), buttons);
    }

    state->setPartyInviteData(data);
}

void PopulateEquipState(QtUiState* state)
{
    if (!state) {
        return;
    }

    UIEquipWnd* const equipWnd = g_windowMgr.m_equipWnd;
    const bool visible = IsGameplayWindowVisible(state, equipWnd);
    state->setEquipVisible(visible);
    if (!visible) {
        state->setEquipGeometry(0, 0, 0, 0);
        state->setEquipMini(false);
        state->setEquipData(QVariantMap{});
        return;
    }

    equipWnd->RefreshLayout();
    state->setEquipGeometry(equipWnd->m_x, equipWnd->m_y, equipWnd->m_w, equipWnd->m_h);
    state->setEquipMini(equipWnd->IsMiniMode());

    UIEquipWnd::DisplayData display{};
    QVariantMap data;
    if (equipWnd->GetDisplayDataForQt(&display)) {
        data.insert(QStringLiteral("title"), QStringLiteral("Equipment"));
        data.insert(QStringLiteral("compact"), equipWnd->IsCompactMode());
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
            button.insert(QStringLiteral("icon"), ToQString(buttonDisplay.icon));
            button.insert(QStringLiteral("visible"), buttonDisplay.visible);
            systemButtons.push_back(button);
        }
        data.insert(QStringLiteral("systemButtons"), systemButtons);

        data.insert(QStringLiteral("previewX"), display.preview.x);
        data.insert(QStringLiteral("previewY"), display.preview.y);
        data.insert(QStringLiteral("previewWidth"), display.preview.width);
        data.insert(QStringLiteral("previewHeight"), display.preview.height);
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
            entry.insert(QStringLiteral("slotName"), ToQString(slot.slotName));
            entry.insert(QStringLiteral("cardX"), equipWnd->m_x + slot.card.x);
            entry.insert(QStringLiteral("cardY"), equipWnd->m_y + slot.card.y);
            entry.insert(QStringLiteral("cardWidth"), slot.card.width);
            entry.insert(QStringLiteral("cardHeight"), slot.card.height);
            entry.insert(QStringLiteral("nameX"), equipWnd->m_x + slot.name.x);
            entry.insert(QStringLiteral("nameY"), equipWnd->m_y + slot.name.y);
            entry.insert(QStringLiteral("nameWidth"), slot.name.width);
            entry.insert(QStringLiteral("nameHeight"), slot.name.height);
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
    const bool visible = IsGameplayWindowVisible(state, skillWnd);
    state->setSkillListVisible(visible);
    if (!visible) {
        state->setSkillListGeometry(0, 0, 0, 0);
        state->setSkillListData(QVariantMap{});
        return;
    }

    state->setSkillListGeometry(skillWnd->m_x, skillWnd->m_y, skillWnd->m_w, skillWnd->m_h);

    const QVariantMap treeData = skillWnd->GetTreeDataForQt();
    if (!treeData.isEmpty()) { state->setSkillListData(treeData); return; }

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

QVariantMap BuildItemDetailData(const UIItemInfoWnd* itemInfoWnd)
{
    UIItemInfoWnd::DisplayData display{};
    QVariantMap data;
    if (itemInfoWnd->GetDisplayDataForQt(&display)) {
        data.insert(QStringLiteral("title"), ToQString(display.title));
        data.insert(QStringLiteral("itemId"), static_cast<uint>(display.itemId));
        data.insert(QStringLiteral("identified"), display.identified);
        data.insert(QStringLiteral("name"), ToQString(display.name));
        data.insert(QStringLiteral("previewUsesCollection"), display.previewUsesCollection);
        data.insert(QStringLiteral("description"), ToQString(display.description));
        const auto localRect=[&](const RECT& r) {
            return QVariantMap{{"x",static_cast<int>(r.left-itemInfoWnd->m_x)},{"y",static_cast<int>(r.top-itemInfoWnd->m_y)},
                {"width",static_cast<int>(r.right-r.left)},{"height",static_cast<int>(r.bottom-r.top)}};
        };
        data.insert(QStringLiteral("previewRect"),localRect(display.previewRect));
        data.insert(QStringLiteral("nameRect"),localRect(display.nameRect));
        data.insert(QStringLiteral("descriptionRect"),localRect(display.descriptionRect));
        data.insert(QStringLiteral("descriptionGeneration"),display.descriptionGeneration);
        data.insert(QStringLiteral("slotTooltip"),ToQString(display.slotTooltip));
        data.insert(QStringLiteral("slotTooltipX"),static_cast<int>(display.slotTooltipAnchor.left));
        data.insert(QStringLiteral("slotTooltipY"),static_cast<int>(display.slotTooltipAnchor.top));

        QVariantList detailLines;
        detailLines.reserve(static_cast<qsizetype>(display.detailLines.size()));
        for (const std::string& line : display.detailLines) {
            detailLines.push_back(ToQString(line));
        }
        data.insert(QStringLiteral("detailLines"), detailLines);

        QVariantMap closeButton;
        closeButton.insert(QStringLiteral("x"), display.closeButton.x);
        closeButton.insert(QStringLiteral("y"), display.closeButton.y);
        closeButton.insert(QStringLiteral("width"), display.closeButton.width);
        closeButton.insert(QStringLiteral("height"), display.closeButton.height);
        closeButton.insert(QStringLiteral("hovered"), display.closeButton.hovered);
        closeButton.insert(QStringLiteral("pressed"), display.closeButton.pressed);
        closeButton.insert(QStringLiteral("label"), ToQString(display.closeButton.label));
        data.insert(QStringLiteral("closeButton"), closeButton);

        QVariantMap graphicsButton;
        graphicsButton.insert(QStringLiteral("x"), display.graphicsButton.x);
        graphicsButton.insert(QStringLiteral("y"), display.graphicsButton.y);
        graphicsButton.insert(QStringLiteral("width"), display.graphicsButton.width);
        graphicsButton.insert(QStringLiteral("height"), display.graphicsButton.height);
        graphicsButton.insert(QStringLiteral("hovered"), display.graphicsButton.hovered);
        graphicsButton.insert(QStringLiteral("pressed"), display.graphicsButton.pressed);
        graphicsButton.insert(QStringLiteral("enabled"), display.graphicsButton.enabled);
        graphicsButton.insert(QStringLiteral("visible"), display.graphicsButton.visible);
        graphicsButton.insert(QStringLiteral("label"), ToQString(display.graphicsButton.label));
        data.insert(QStringLiteral("graphicsButton"), graphicsButton);

        QVariantList slotList;
        slotList.reserve(static_cast<qsizetype>(display.slotEntries.size()));
        for (const UIItemInfoWnd::DisplaySlot& slot : display.slotEntries) {
            QVariantMap entry;
            entry.insert(QStringLiteral("x"), slot.x);
            entry.insert(QStringLiteral("y"), slot.y);
            entry.insert(QStringLiteral("width"), slot.width);
            entry.insert(QStringLiteral("height"), slot.height);
            entry.insert(QStringLiteral("available"), slot.available);
            entry.insert(QStringLiteral("occupied"), slot.occupied);
            entry.insert(QStringLiteral("hovered"), slot.hovered);
            entry.insert(QStringLiteral("itemId"), static_cast<uint>(slot.itemId));
            entry.insert(QStringLiteral("tooltip"), ToQString(slot.tooltip));
            slotList.push_back(entry);
        }
        data.insert(QStringLiteral("slots"), slotList);
    }
    return data;
}

void PopulateItemInfoState(QtUiState* state)
{
    if(!state)return;
    auto* main=g_windowMgr.m_itemInfoWnd;
    auto* card=g_windowMgr.m_cardInfoWnd;
    if(main)main->RefreshBoundItem();
    if(card)card->RefreshBoundItem();
    auto* obtain=g_windowMgr.m_itemObtainWnd;
    int layer=40,mainLayer=40,cardLayer=40,obtainLayer=40;
    for(auto* window:g_windowMgr.m_children) {
        if(window==main){mainLayer=layer++;}
        else if(window==card){cardLayer=layer++;}
        else if(window==obtain){obtainLayer=layer++;}
    }
    const bool mainVisible=IsGameplayWindowVisible(state,main)&&main->HasContent();
    state->setItemInfoVisible(mainVisible);
    if(mainVisible) {
        state->setItemInfoGeometry(main->m_x,main->m_y,main->m_w,main->m_h);
        auto data=BuildItemDetailData(main);data.insert("layer",mainLayer);
        state->setItemInfoData(data);
    } else {
        state->setItemInfoGeometry(0,0,0,0);
        state->setItemInfoData(QVariantMap{});
    }
    if(IsGameplayWindowVisible(state,card)&&card->HasItem()) {
        auto data=BuildItemDetailData(card);
        data.insert("visible",true);data.insert("x",card->m_x);data.insert("y",card->m_y);
        data.insert("width",card->m_w);data.insert("height",card->m_h);data.insert("layer",cardLayer);
        state->setCardInfoData(data);
    } else state->setCardInfoData(QVariantMap{});
    UINotifyItemObtainWnd::DisplayData display;
    if(IsGameplayWindowVisible(state,obtain))obtain->RefreshContent();
    if(IsGameplayWindowVisible(state,obtain)&&obtain->GetDisplayDataForQt(&display)) {
        QVariantMap data{{"visible",true},{"x",obtain->m_x},{"y",obtain->m_y},
            {"width",obtain->m_w},{"height",obtain->m_h},{"layer",obtainLayer},
            {"itemId",display.itemId},{"identified",display.identified},
            {"text",ToQString(display.text)},{"sizingText",ToQString(display.sizingText)}};
        QVariantList runs;
        for(const auto& run:display.runs)runs.push_back(QVariantMap{{"text",ToQString(run.text)},
            {"x",run.x},{"y",run.y},{"width",run.width},{"colorARGB",run.color},
            {"color",QColor::fromRgba(run.color)}});
        data.insert("runs",runs);state->setItemObtainData(data);
    } else state->setItemObtainData(QVariantMap{});
}

void PopulateSkillDescribeState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UISkillDescribeWnd* const skillDescribeWnd = g_windowMgr.m_skillDescribeWnd;
    const bool visible = IsGameplayWindowVisible(state, skillDescribeWnd) && skillDescribeWnd->HasSkill();
    state->setSkillDescribeVisible(visible);
    if (!visible) {
        state->setSkillDescribeGeometry(0, 0, 0, 0);
        state->setSkillDescribeData(QVariantMap{});
        return;
    }

    state->setSkillDescribeGeometry(skillDescribeWnd->m_x, skillDescribeWnd->m_y, skillDescribeWnd->m_w, skillDescribeWnd->m_h);

    UISkillDescribeWnd::DisplayData display{};
    QVariantMap data;
    if (skillDescribeWnd->GetDisplayDataForQt(&display)) {
        data.insert(QStringLiteral("title"), ToQString(display.title));
        data.insert(QStringLiteral("skillId"), display.skillId);
        data.insert(QStringLiteral("name"), ToQString(display.name));
        data.insert(QStringLiteral("description"), ToQString(display.description));
        data.insert(QStringLiteral("descriptionX"), display.descriptionX);
        data.insert(QStringLiteral("descriptionY"), display.descriptionY);
        data.insert(QStringLiteral("descriptionWidth"), display.descriptionWidth);
        data.insert(QStringLiteral("descriptionHeight"), display.descriptionHeight);
        data.insert(QStringLiteral("descriptionContentHeight"), display.descriptionContentHeight);
        data.insert(QStringLiteral("descriptionScrollOffset"), display.descriptionScrollOffset);

        QVariantList detailLines;
        detailLines.reserve(static_cast<qsizetype>(display.detailLines.size()));
        for (const std::string& line : display.detailLines) {
            if (!line.empty()) {
                detailLines.push_back(ToQString(line));
            }
        }
        data.insert(QStringLiteral("detailLines"), detailLines);

        QVariantMap closeButton;
        closeButton.insert(QStringLiteral("x"), display.closeButton.x);
        closeButton.insert(QStringLiteral("y"), display.closeButton.y);
        closeButton.insert(QStringLiteral("width"), display.closeButton.width);
        closeButton.insert(QStringLiteral("height"), display.closeButton.height);
        closeButton.insert(QStringLiteral("hovered"), display.closeButton.hovered);
        closeButton.insert(QStringLiteral("pressed"), display.closeButton.pressed);
        closeButton.insert(QStringLiteral("label"), ToQString(display.closeButton.label));
        data.insert(QStringLiteral("closeButton"), closeButton);

        QVariantMap descriptionScrollBar;
        descriptionScrollBar.insert(QStringLiteral("visible"), display.descriptionScrollBar.visible);
        descriptionScrollBar.insert(QStringLiteral("trackX"), display.descriptionScrollBar.trackX);
        descriptionScrollBar.insert(QStringLiteral("trackY"), display.descriptionScrollBar.trackY);
        descriptionScrollBar.insert(QStringLiteral("trackWidth"), display.descriptionScrollBar.trackWidth);
        descriptionScrollBar.insert(QStringLiteral("trackHeight"), display.descriptionScrollBar.trackHeight);
        descriptionScrollBar.insert(QStringLiteral("thumbX"), display.descriptionScrollBar.thumbX);
        descriptionScrollBar.insert(QStringLiteral("thumbY"), display.descriptionScrollBar.thumbY);
        descriptionScrollBar.insert(QStringLiteral("thumbWidth"), display.descriptionScrollBar.thumbWidth);
        descriptionScrollBar.insert(QStringLiteral("thumbHeight"), display.descriptionScrollBar.thumbHeight);
        data.insert(QStringLiteral("descriptionScrollBar"), descriptionScrollBar);
    }
    state->setSkillDescribeData(data);
}

void PopulateItemCollectionState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIItemCollectionWnd* const itemCollectionWnd = g_windowMgr.m_itemCollectionWnd;
    const bool visible = IsGameplayWindowVisible(state, itemCollectionWnd) && itemCollectionWnd->HasItem();
    state->setItemCollectionVisible(visible);
    if (!visible) {
        state->setItemCollectionGeometry(0, 0, 0, 0);
        state->setItemCollectionData(QVariantMap{});
        return;
    }

    state->setItemCollectionGeometry(itemCollectionWnd->m_x, itemCollectionWnd->m_y, itemCollectionWnd->m_w, itemCollectionWnd->m_h);

    UIItemCollectionWnd::DisplayData display{};
    QVariantMap data;
    if (itemCollectionWnd->GetDisplayDataForQt(&display)) {
        data.insert(QStringLiteral("title"), ToQString(display.title));
        data.insert(QStringLiteral("itemId"), static_cast<uint>(display.itemId));
        data.insert(QStringLiteral("mainUsesIllust"), display.mainUsesIllust);
        data.insert(QStringLiteral("mainUsesCollection"), display.mainUsesCollection);

        QVariantMap closeButton;
        closeButton.insert(QStringLiteral("x"), display.closeButton.x);
        closeButton.insert(QStringLiteral("y"), display.closeButton.y);
        closeButton.insert(QStringLiteral("width"), display.closeButton.width);
        closeButton.insert(QStringLiteral("height"), display.closeButton.height);
        closeButton.insert(QStringLiteral("hovered"), display.closeButton.hovered);
        closeButton.insert(QStringLiteral("pressed"), display.closeButton.pressed);
        closeButton.insert(QStringLiteral("label"), ToQString(display.closeButton.label));
        data.insert(QStringLiteral("closeButton"), closeButton);
    }
    state->setItemCollectionData(data);
}

void PopulateItemIdentifyState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIItemIdentifyWnd* const itemIdentifyWnd = g_windowMgr.m_itemIdentifyWnd;
    const bool visible = IsGameplayWindowVisible(state, itemIdentifyWnd);
    state->setItemIdentifyVisible(visible);
    if (!visible) {
        state->setItemIdentifyGeometry(0, 0, 0, 0);
        state->setItemIdentifyData(QVariantMap{});
        return;
    }

    state->setItemIdentifyGeometry(itemIdentifyWnd->m_x, itemIdentifyWnd->m_y, itemIdentifyWnd->m_w, itemIdentifyWnd->m_h);

    UIItemIdentifyWnd::DisplayData display{};
    QVariantMap data;
    if (itemIdentifyWnd->GetDisplayDataForQt(&display)) {
        data.insert(QStringLiteral("title"), ToQString(display.title));
        data.insert(QStringLiteral("emptyText"), ToQString(display.emptyText));

        QVariantMap closeButton;
        closeButton.insert(QStringLiteral("x"), display.closeButton.x);
        closeButton.insert(QStringLiteral("y"), display.closeButton.y);
        closeButton.insert(QStringLiteral("width"), display.closeButton.width);
        closeButton.insert(QStringLiteral("height"), display.closeButton.height);
        closeButton.insert(QStringLiteral("hovered"), display.closeButton.hovered);
        closeButton.insert(QStringLiteral("pressed"), display.closeButton.pressed);
        closeButton.insert(QStringLiteral("label"), ToQString(display.closeButton.label));
        data.insert(QStringLiteral("closeButton"), closeButton);

        QVariantMap okButton;
        okButton.insert(QStringLiteral("x"), display.okButton.x);
        okButton.insert(QStringLiteral("y"), display.okButton.y);
        okButton.insert(QStringLiteral("width"), display.okButton.width);
        okButton.insert(QStringLiteral("height"), display.okButton.height);
        okButton.insert(QStringLiteral("hovered"), display.okButton.hovered);
        okButton.insert(QStringLiteral("pressed"), display.okButton.pressed);
        okButton.insert(QStringLiteral("enabled"), display.okButton.enabled);
        okButton.insert(QStringLiteral("label"), ToQString(display.okButton.label));
        data.insert(QStringLiteral("okButton"), okButton);

        QVariantMap cancelButton;
        cancelButton.insert(QStringLiteral("x"), display.cancelButton.x);
        cancelButton.insert(QStringLiteral("y"), display.cancelButton.y);
        cancelButton.insert(QStringLiteral("width"), display.cancelButton.width);
        cancelButton.insert(QStringLiteral("height"), display.cancelButton.height);
        cancelButton.insert(QStringLiteral("hovered"), display.cancelButton.hovered);
        cancelButton.insert(QStringLiteral("pressed"), display.cancelButton.pressed);
        cancelButton.insert(QStringLiteral("label"), ToQString(display.cancelButton.label));
        data.insert(QStringLiteral("cancelButton"), cancelButton);

        QVariantList rows;
        rows.reserve(static_cast<qsizetype>(display.rows.size()));
        for (const UIItemIdentifyWnd::DisplayRow& row : display.rows) {
            QVariantMap entry;
            entry.insert(QStringLiteral("itemIndex"), static_cast<uint>(row.itemIndex));
            entry.insert(QStringLiteral("itemId"), static_cast<uint>(row.itemId));
            entry.insert(QStringLiteral("identified"), row.identified);
            entry.insert(QStringLiteral("x"), row.x);
            entry.insert(QStringLiteral("y"), row.y);
            entry.insert(QStringLiteral("width"), row.width);
            entry.insert(QStringLiteral("height"), row.height);
            entry.insert(QStringLiteral("hovered"), row.hovered);
            entry.insert(QStringLiteral("selected"), row.selected);
            entry.insert(QStringLiteral("count"), row.count);
            entry.insert(QStringLiteral("label"), ToQString(row.label));
            rows.push_back(entry);
        }
        data.insert(QStringLiteral("rows"), rows);

        QVariantMap scrollBar;
        scrollBar.insert(QStringLiteral("visible"), display.scrollBar.visible);
        scrollBar.insert(QStringLiteral("trackX"), display.scrollBar.trackX);
        scrollBar.insert(QStringLiteral("trackY"), display.scrollBar.trackY);
        scrollBar.insert(QStringLiteral("trackWidth"), display.scrollBar.trackWidth);
        scrollBar.insert(QStringLiteral("trackHeight"), display.scrollBar.trackHeight);
        scrollBar.insert(QStringLiteral("thumbX"), display.scrollBar.thumbX);
        scrollBar.insert(QStringLiteral("thumbY"), display.scrollBar.thumbY);
        scrollBar.insert(QStringLiteral("thumbWidth"), display.scrollBar.thumbWidth);
        scrollBar.insert(QStringLiteral("thumbHeight"), display.scrollBar.thumbHeight);
        data.insert(QStringLiteral("scrollBar"), scrollBar);
    }
    state->setItemIdentifyData(data);
}

void PopulateSkillSelectState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UISkillSelectWnd* const skillSelectWnd = g_windowMgr.m_skillSelectWnd;
    const bool visible = IsGameplayWindowVisible(state, skillSelectWnd);
    state->setSkillSelectVisible(visible);
    if (!visible) {
        state->setSkillSelectGeometry(0, 0, 0, 0);
        state->setSkillSelectData(QVariantMap{});
        return;
    }

    state->setSkillSelectGeometry(skillSelectWnd->m_x, skillSelectWnd->m_y, skillSelectWnd->m_w, skillSelectWnd->m_h);

    UISkillSelectWnd::DisplayData display{};
    QVariantMap data;
    if (skillSelectWnd->GetDisplayDataForQt(&display)) {
        data.insert(QStringLiteral("title"), ToQString(display.title));
        data.insert(QStringLiteral("emptyText"), ToQString(display.emptyText));
        data.insert(QStringLiteral("recipeDescription"), ToQString(display.recipeDescription));
        data.insert(QStringLiteral("recipeHeight"), display.recipeHeight);
        data.insert(QStringLiteral("productionConfirmation"), display.productionConfirmation);
        QVariantList materialSlots;
        for (const auto& slot : display.materialSlots) {
            materialSlots.push_back(QVariantMap{
                {QStringLiteral("itemId"), slot.itemId}, {QStringLiteral("x"), slot.x},
                {QStringLiteral("y"), slot.y}, {QStringLiteral("width"), slot.width},
                {QStringLiteral("height"), slot.height},
            });
        }
        data.insert(QStringLiteral("materialSlots"), materialSlots);

        QVariantMap closeButton;
        closeButton.insert(QStringLiteral("x"), display.closeButton.x);
        closeButton.insert(QStringLiteral("y"), display.closeButton.y);
        closeButton.insert(QStringLiteral("width"), display.closeButton.width);
        closeButton.insert(QStringLiteral("height"), display.closeButton.height);
        closeButton.insert(QStringLiteral("hovered"), display.closeButton.hovered);
        closeButton.insert(QStringLiteral("pressed"), display.closeButton.pressed);
        closeButton.insert(QStringLiteral("label"), ToQString(display.closeButton.label));
        data.insert(QStringLiteral("closeButton"), closeButton);

        QVariantMap okButton;
        okButton.insert(QStringLiteral("x"), display.okButton.x);
        okButton.insert(QStringLiteral("y"), display.okButton.y);
        okButton.insert(QStringLiteral("width"), display.okButton.width);
        okButton.insert(QStringLiteral("height"), display.okButton.height);
        okButton.insert(QStringLiteral("hovered"), display.okButton.hovered);
        okButton.insert(QStringLiteral("pressed"), display.okButton.pressed);
        okButton.insert(QStringLiteral("enabled"), display.okButton.enabled);
        okButton.insert(QStringLiteral("label"), ToQString(display.okButton.label));
        data.insert(QStringLiteral("okButton"), okButton);

        QVariantMap cancelButton;
        cancelButton.insert(QStringLiteral("x"), display.cancelButton.x);
        cancelButton.insert(QStringLiteral("y"), display.cancelButton.y);
        cancelButton.insert(QStringLiteral("width"), display.cancelButton.width);
        cancelButton.insert(QStringLiteral("height"), display.cancelButton.height);
        cancelButton.insert(QStringLiteral("hovered"), display.cancelButton.hovered);
        cancelButton.insert(QStringLiteral("pressed"), display.cancelButton.pressed);
        cancelButton.insert(QStringLiteral("label"), ToQString(display.cancelButton.label));
        data.insert(QStringLiteral("cancelButton"), cancelButton);

        QVariantList rows;
        rows.reserve(static_cast<qsizetype>(display.rows.size()));
        for (const UISkillSelectWnd::DisplayRow& row : display.rows) {
            QVariantMap entry;
            entry.insert(QStringLiteral("itemId"), static_cast<uint>(row.itemId));
            entry.insert(QStringLiteral("identified"), row.identified);
            entry.insert(QStringLiteral("showIcon"), row.showIcon);
            entry.insert(QStringLiteral("x"), row.x);
            entry.insert(QStringLiteral("y"), row.y);
            entry.insert(QStringLiteral("width"), row.width);
            entry.insert(QStringLiteral("height"), row.height);
            entry.insert(QStringLiteral("hovered"), row.hovered);
            entry.insert(QStringLiteral("selected"), row.selected);
            entry.insert(QStringLiteral("count"), row.count);
            entry.insert(QStringLiteral("label"), ToQString(row.label));
            rows.push_back(entry);
        }
        data.insert(QStringLiteral("rows"), rows);

        QVariantMap scrollBar;
        scrollBar.insert(QStringLiteral("visible"), display.scrollBar.visible);
        scrollBar.insert(QStringLiteral("trackX"), display.scrollBar.trackX);
        scrollBar.insert(QStringLiteral("trackY"), display.scrollBar.trackY);
        scrollBar.insert(QStringLiteral("trackWidth"), display.scrollBar.trackWidth);
        scrollBar.insert(QStringLiteral("trackHeight"), display.scrollBar.trackHeight);
        scrollBar.insert(QStringLiteral("thumbX"), display.scrollBar.thumbX);
        scrollBar.insert(QStringLiteral("thumbY"), display.scrollBar.thumbY);
        scrollBar.insert(QStringLiteral("thumbWidth"), display.scrollBar.thumbWidth);
        scrollBar.insert(QStringLiteral("thumbHeight"), display.scrollBar.thumbHeight);
        data.insert(QStringLiteral("scrollBar"), scrollBar);
    }
    state->setSkillSelectData(data);
}

void PopulateItemCompositionState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIItemCompositionWnd* const itemCompositionWnd = g_windowMgr.m_itemCompositionWnd;
    const bool visible = IsGameplayWindowVisible(state, itemCompositionWnd);
    state->setItemCompositionVisible(visible);
    if (!visible) {
        state->setItemCompositionGeometry(0, 0, 0, 0);
        state->setItemCompositionData(QVariantMap{});
        return;
    }

    state->setItemCompositionGeometry(itemCompositionWnd->m_x, itemCompositionWnd->m_y, itemCompositionWnd->m_w, itemCompositionWnd->m_h);

    UIItemCompositionWnd::DisplayData display{};
    QVariantMap data;
    if (itemCompositionWnd->GetDisplayDataForQt(&display)) {
        data.insert(QStringLiteral("title"), ToQString(display.title));
        data.insert(QStringLiteral("emptyText"), ToQString(display.emptyText));

        QVariantMap closeButton;
        closeButton.insert(QStringLiteral("x"), display.closeButton.x);
        closeButton.insert(QStringLiteral("y"), display.closeButton.y);
        closeButton.insert(QStringLiteral("width"), display.closeButton.width);
        closeButton.insert(QStringLiteral("height"), display.closeButton.height);
        closeButton.insert(QStringLiteral("hovered"), display.closeButton.hovered);
        closeButton.insert(QStringLiteral("pressed"), display.closeButton.pressed);
        closeButton.insert(QStringLiteral("label"), ToQString(display.closeButton.label));
        data.insert(QStringLiteral("closeButton"), closeButton);

        QVariantMap okButton;
        okButton.insert(QStringLiteral("x"), display.okButton.x);
        okButton.insert(QStringLiteral("y"), display.okButton.y);
        okButton.insert(QStringLiteral("width"), display.okButton.width);
        okButton.insert(QStringLiteral("height"), display.okButton.height);
        okButton.insert(QStringLiteral("hovered"), display.okButton.hovered);
        okButton.insert(QStringLiteral("pressed"), display.okButton.pressed);
        okButton.insert(QStringLiteral("enabled"), display.okButton.enabled);
        okButton.insert(QStringLiteral("label"), ToQString(display.okButton.label));
        data.insert(QStringLiteral("okButton"), okButton);

        QVariantMap cancelButton;
        cancelButton.insert(QStringLiteral("x"), display.cancelButton.x);
        cancelButton.insert(QStringLiteral("y"), display.cancelButton.y);
        cancelButton.insert(QStringLiteral("width"), display.cancelButton.width);
        cancelButton.insert(QStringLiteral("height"), display.cancelButton.height);
        cancelButton.insert(QStringLiteral("hovered"), display.cancelButton.hovered);
        cancelButton.insert(QStringLiteral("pressed"), display.cancelButton.pressed);
        cancelButton.insert(QStringLiteral("label"), ToQString(display.cancelButton.label));
        data.insert(QStringLiteral("cancelButton"), cancelButton);

        QVariantList rows;
        rows.reserve(static_cast<qsizetype>(display.rows.size()));
        for (const UIItemCompositionWnd::DisplayRow& row : display.rows) {
            QVariantMap entry;
            entry.insert(QStringLiteral("itemIndex"), static_cast<uint>(row.itemIndex));
            entry.insert(QStringLiteral("itemId"), static_cast<uint>(row.itemId));
            entry.insert(QStringLiteral("identified"), row.identified);
            entry.insert(QStringLiteral("x"), row.x);
            entry.insert(QStringLiteral("y"), row.y);
            entry.insert(QStringLiteral("width"), row.width);
            entry.insert(QStringLiteral("height"), row.height);
            entry.insert(QStringLiteral("hovered"), row.hovered);
            entry.insert(QStringLiteral("selected"), row.selected);
            entry.insert(QStringLiteral("count"), row.count);
            entry.insert(QStringLiteral("label"), ToQString(row.label));
            rows.push_back(entry);
        }
        data.insert(QStringLiteral("rows"), rows);

        QVariantMap scrollBar;
        scrollBar.insert(QStringLiteral("visible"), display.scrollBar.visible);
        scrollBar.insert(QStringLiteral("trackX"), display.scrollBar.trackX);
        scrollBar.insert(QStringLiteral("trackY"), display.scrollBar.trackY);
        scrollBar.insert(QStringLiteral("trackWidth"), display.scrollBar.trackWidth);
        scrollBar.insert(QStringLiteral("trackHeight"), display.scrollBar.trackHeight);
        scrollBar.insert(QStringLiteral("thumbX"), display.scrollBar.thumbX);
        scrollBar.insert(QStringLiteral("thumbY"), display.scrollBar.thumbY);
        scrollBar.insert(QStringLiteral("thumbWidth"), display.scrollBar.thumbWidth);
        scrollBar.insert(QStringLiteral("thumbHeight"), display.scrollBar.thumbHeight);
        data.insert(QStringLiteral("scrollBar"), scrollBar);
    }
    state->setItemCompositionData(data);
}

void PopulateOptionState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIOptionWnd* const optionWnd = g_windowMgr.m_optionWnd;
    const bool visible = IsGameplayWindowVisible(state, optionWnd);
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
        data.insert(QStringLiteral("keybindingBar"), display.keybindingBar);
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

        QVariantMap applyButton;
        applyButton.insert(QStringLiteral("x"), display.applyButton.x);
        applyButton.insert(QStringLiteral("y"), display.applyButton.y);
        applyButton.insert(QStringLiteral("width"), display.applyButton.width);
        applyButton.insert(QStringLiteral("height"), display.applyButton.height);
        applyButton.insert(QStringLiteral("visible"), display.applyButton.visible);
        applyButton.insert(QStringLiteral("label"), ToQString(display.applyButton.label));
        data.insert(QStringLiteral("applyButton"), applyButton);

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
            entry.insert(QStringLiteral("minValue"), slider.minValue);
            entry.insert(QStringLiteral("maxValue"), slider.maxValue);
            entry.insert(QStringLiteral("label"), ToQString(slider.label));
            entry.insert(QStringLiteral("valueText"), ToQString(slider.valueText));
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

        QVariantList keybindings;
        keybindings.reserve(static_cast<qsizetype>(display.keybindings.size()));
        for (const UIOptionWnd::DisplayKeybinding& keybinding : display.keybindings) {
            QVariantMap entry;
            entry.insert(QStringLiteral("x"), keybinding.x);
            entry.insert(QStringLiteral("y"), keybinding.y);
            entry.insert(QStringLiteral("width"), keybinding.width);
            entry.insert(QStringLiteral("height"), keybinding.height);
            entry.insert(QStringLiteral("barIndex"), keybinding.barIndex);
            entry.insert(QStringLiteral("column"), keybinding.column);
            entry.insert(QStringLiteral("capturing"), keybinding.capturing);
            entry.insert(QStringLiteral("label"), ToQString(keybinding.label));
            entry.insert(QStringLiteral("value"), ToQString(keybinding.value));
            keybindings.push_back(entry);
        }
        data.insert(QStringLiteral("keybindings"), keybindings);
    }
    state->setOptionData(data);
}

void PopulateMinimapState(QtUiState* state)
{
    if (!state) {
        return;
    }

    const UIRoMapWnd* const minimapWnd = g_windowMgr.m_roMapWnd;
    const bool visible = IsGameplayWindowVisible(state, minimapWnd) && minimapWnd->HasLoadedMinimap();
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
            entry.insert(QStringLiteral("compass"), marker.compass);
            entry.insert(QStringLiteral("color"), QStringLiteral("#%1").arg(marker.color & 0x00FFFFFFu, 6, 16, QLatin1Char('0')));
            markers.push_back(entry);
        }
        data.insert(QStringLiteral("markers"), markers);
    }
    state->setMinimapData(data);
}

void PopulateStatusIconsState(QtUiState* state, int mouseX, int mouseY)
{
    if (!state) {
        return;
    }

    const u32 now = g_session.GetServerTime();
    g_session.PruneExpiredStatusIcons(now);

    QVariantList icons;
    const std::vector<ACTIVE_STATUS_ICON>& activeIcons = g_session.GetActiveStatusIcons();
    std::vector<const ACTIVE_STATUS_ICON*> orderedIcons;
    orderedIcons.reserve(activeIcons.size());
    std::vector<int> drawnIds;
    drawnIds.reserve(activeIcons.size());
    // Same priority walk and status-42 dedup as CRagEffect::SpawnStatusState.
    for (int priority = 0; priority < 4; ++priority) {
        for (const ACTIVE_STATUS_ICON& icon : activeIcons) {
            if (!FindStatusStateIcon(priority, icon.statusType)) {
                continue;
            }
            const int drawId = icon.statusType == 42 ? 12 : icon.statusType;
            bool alreadyDrawn = false;
            for (int drawnId : drawnIds) {
                if (drawnId == drawId) {
                    alreadyDrawn = true;
                    break;
                }
            }
            if (alreadyDrawn) {
                continue;
            }
            orderedIcons.push_back(&icon);
            drawnIds.push_back(drawId);
        }
    }
    // Ref StatusState draws a tile only when GetStateFileName returns a path.
    // Status 46 (SI_ACTIONDELAY, the aftercast delay) is in the state list but
    // has no HighPriest icon, so it must not get a fallback tile.

    icons.reserve(static_cast<qsizetype>(orderedIcons.size()));
    const int hoveredIndex = FindHoveredStatusIconIndex(state, mouseX, mouseY);
    for (const ACTIVE_STATUS_ICON* iconPtr : orderedIcons) {
        const ACTIVE_STATUS_ICON& entry = *iconPtr;
        const qtui::StatusIconCatalogEntry* const catalogEntry = qtui::FindStatusIconCatalogEntry(entry.statusType);
        const int skillId = catalogEntry ? catalogEntry->skillId : 0;
        const u32 remainingMs = (entry.hasTimer && entry.expireServerTime > now)
            ? (entry.expireServerTime - now)
            : 0;
        const QString label = ResolveStatusIconLabel(entry.statusType, skillId);

        QVariantMap icon;
        icon.insert(QStringLiteral("statusType"), entry.statusType);
        icon.insert(QStringLiteral("skillId"), skillId);
        icon.insert(QStringLiteral("name"), label);
        icon.insert(QStringLiteral("shortName"), BuildStatusIconShortLabel(label));
        icon.insert(QStringLiteral("timed"), entry.hasTimer);
        icon.insert(QStringLiteral("remainingMs"), static_cast<int>(remainingMs));
        icon.insert(QStringLiteral("hovered"), hoveredIndex == icons.size());
        icons.push_back(icon);
    }

    state->setStatusIcons(icons);
}

bool IsMonsterLikeHoverActor(const CGameActor* actor)
{
    if (!actor || actor->m_isPc != 0) {
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

const NamePair* FindCachedHoverActorNameEntry(const CGameMode& mode, const CGameActor* actor)
{
    if (!actor) {
        return nullptr;
    }

    const auto cachedNameIt = mode.m_actorNameListByGID.find(actor->m_gid);
    if (cachedNameIt == mode.m_actorNameListByGID.end()) {
        return nullptr;
    }

    return &cachedNameIt->second;
}

std::string ResolveFallbackPartyNameForHoverActor(const std::string& actorName, const CGameActor* actor)
{
    if (!actor || g_session.m_partyName.empty()) {
        return std::string();
    }

    if (actor->m_gid == g_session.m_gid || actor->m_gid == g_session.m_aid) {
        return g_session.m_partyName;
    }

    if (actorName.empty()) {
        return std::string();
    }

    const std::list<FRIEND_INFO>& partyMembers = g_session.GetPartyList();
    for (const FRIEND_INFO& member : partyMembers) {
        if (member.characterName == actorName) {
            return g_session.m_partyName;
        }
    }

    return std::string();
}

std::string BuildHoverActorLabel(const std::string& actorName, const NamePair* cachedEntry, const CGameActor* actor)
{
    if (actorName.empty()) {
        return std::string();
    }

    std::string label = actorName;
    std::string partyName = cachedEntry ? cachedEntry->partyName : std::string();
    if (partyName.empty()) {
        partyName = ResolveFallbackPartyNameForHoverActor(actorName, actor);
    }
    if (!partyName.empty()) {
        label += " (";
        label += partyName;
        label += ")";
    }

    const bool localGuild=actor && (actor->m_gid==g_session.m_gid || actor->m_gid==g_session.m_aid)
        && g_session.m_localGuild.characterId==g_session.m_gid;
    const std::string guildName = localGuild?g_session.m_localGuild.name:cachedEntry ? cachedEntry->guildName : std::string();
    if (!guildName.empty()) {
        label += "\n";
        label += guildName;

        const std::string guildTitle = localGuild?g_session.m_localGuild.title:cachedEntry ? cachedEntry->guildTitle : std::string();
        if (!guildTitle.empty()) {
            label += " [";
            label += guildTitle;
            label += "]";
        }
    }

    return label;
}

void SendActorNameRequest(CGameMode& mode, u32 gid)
{
    if (gid == 0) {
        return;
    }

    const u32 now = GetTickCount();
    const auto timerIt = mode.m_actorNameByGIDReqTimer.find(gid);
    if (timerIt != mode.m_actorNameByGIDReqTimer.end() && now - timerIt->second < kHoverNameRequestCooldownMs) {
        return;
    }

    bool sent = false;
    if (ro::net::IsLegacyMapGameplaySendProfile()) {
        PACKET_CZ_REQNAME_LEGACY packet{};
        packet.PacketType = ro::net::GetActiveMapGameplaySendProfile().getCharNameRequest;
        packet.GID = gid;
        sent = CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), static_cast<int>(sizeof(packet)));
    } else {
        PACKET_CZ_REQNAME2 packet{};
        packet.PacketType = ro::net::GetActiveMapGameplaySendProfile().getCharNameRequest;
        FillPacketPadding(packet.padding, static_cast<int>(sizeof(packet.padding)));
        packet.GID = gid;
        sent = CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), static_cast<int>(sizeof(packet)));
    }
    if (sent) {
        mode.m_actorNameByGIDReqTimer[gid] = now;
    }
}

std::string ResolveActorLabel(const CGameMode& mode, CGameActor* actor)
{
    if (!actor) {
        return std::string();
    }

    const NamePair* const cachedEntry = FindCachedHoverActorNameEntry(mode, actor);

    if (actor->m_gid == g_session.m_gid) {
        // The local name is known, but party/guild/rank still need the full
        // server name reply. Self-hover must not bypass that request.
        if(!cachedEntry || cachedEntry->name.empty())SendActorNameRequest(const_cast<CGameMode&>(mode),actor->m_gid);
        const char* playerName = g_session.GetPlayerName();
        if (playerName && *playerName) {
            return BuildHoverActorLabel(playerName, cachedEntry, actor);
        }
    }

    if (cachedEntry && !cachedEntry->name.empty()) {
        return BuildHoverActorLabel(cachedEntry->name, cachedEntry, actor);
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

QString ResolveGuildEmblem(CGameMode& mode,const CGameActor* actor)
{
    if(!actor)return {};
    const bool self=(actor->m_gid==g_session.m_gid || actor->m_gid==g_session.m_aid)
        && g_session.m_localGuild.characterId==g_session.m_gid;
    const u32 guild=self?g_session.m_localGuild.guildId:static_cast<u32>(actor->m_gdid);
    const u32 version=self?g_session.m_localGuild.emblemVersion:static_cast<u32>(actor->m_emblemVersion);
    if(!guild || !version)return {};
    const auto found=mode.m_guildEmblems.find(guild);
    if(found!=mode.m_guildEmblems.end() && found->second.version==version) {
        auto& cached=found->second;
        if(!cached.decoded) {
            cached.decoded=true;
            QImage image=QImage::fromData(cached.bitmap.data(),static_cast<int>(cached.bitmap.size()),"BMP");
            if(image.width()==24 && image.height()==24) {
                image=ro::guild::TransparentEmblemImage(image);
                QByteArray png;QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);
                if(image.save(&buffer,"PNG"))cached.imageUri="data:image/png;base64,"+png.toBase64().toStdString();
            }
        }
        return QString::fromStdString(cached.imageUri);
    }
    const u32 now=GetTickCount();
    const auto timer=mode.m_emblemRequestTicks.find(guild);
    if(timer==mode.m_emblemRequestTicks.end() || now-timer->second>=5000) {
        const u8 request[6]={0x51,0x01,static_cast<u8>(guild),static_cast<u8>(guild>>8),static_cast<u8>(guild>>16),static_cast<u8>(guild>>24)};
        if(CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(request),6))mode.m_emblemRequestTicks[guild]=now;
    }
    return {};
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

QVariantMap MakeSpeechBubbleAnchor(const QString& text,
    int centerX,
    int bottomY)
{
    QVariantMap anchor;
    anchor.insert(QStringLiteral("text"), text);
    anchor.insert(QStringLiteral("centerX"), centerX);
    anchor.insert(QStringLiteral("bottomY"), bottomY);
    anchor.insert(QStringLiteral("background"), QStringLiteral("#de333333"));
    anchor.insert(QStringLiteral("foreground"), QStringLiteral("#ffffff"));
    anchor.insert(QStringLiteral("showBubble"), true);
    anchor.insert(QStringLiteral("fontPixelSize"), 14);
    anchor.insert(QStringLiteral("bold"), false);
    anchor.insert(QStringLiteral("wrap"), true);
    anchor.insert(QStringLiteral("maxTextWidth"), kQtSpeechBubbleMaxTextWidth);
    anchor.insert(QStringLiteral("paddingX"), 16);
    anchor.insert(QStringLiteral("paddingY"), 12);
    anchor.insert(QStringLiteral("radius"), 8);
    anchor.insert(QStringLiteral("borderColor"), QStringLiteral("#80686868"));
    anchor.insert(QStringLiteral("z"), 2100);
    return anchor;
}

QVariantMap MakeSkillTalkboxAnchor(const QString& text, int centerX, int bottomY)
{
    QVariantMap anchor = MakeSpeechBubbleAnchor(text, centerX, bottomY);
    anchor.insert(QStringLiteral("background"), QStringLiteral("#00000000"));
    anchor.insert(QStringLiteral("foreground"), QStringLiteral("#ff8080"));
    anchor.insert(QStringLiteral("maxTextWidth"), 490);
    return anchor;
}

QVariantMap MakePartyHpBarAnchor(int centerX, int topY, long long currentHp, long long maxHp, const PlayerVitalsScreenMetrics& metrics)
{
    QVariantMap anchor;
    anchor.insert(QStringLiteral("kind"), QStringLiteral("partyHpBar"));
    const int width=metrics.barWidth+metrics.border*2;
    anchor.insert(QStringLiteral("x"), centerX - width/2);
    anchor.insert(QStringLiteral("y"), topY);
    anchor.insert(QStringLiteral("width"), width);
    anchor.insert(QStringLiteral("height"), metrics.barHeight+metrics.border*2);
    anchor.insert(QStringLiteral("border"), metrics.border);
    anchor.insert(QStringLiteral("currentHp"), currentHp);
    anchor.insert(QStringLiteral("maxHp"), maxHp);
    anchor.insert(QStringLiteral("z"), -1);
    return anchor;
}

QVariantMap MakeUiItemAnchor(const shopui::ItemHoverInfo& hoverInfo)
{
    const int logicalCenterX = hoverInfo.anchorRect.left + ((hoverInfo.anchorRect.right - hoverInfo.anchorRect.left) / 2);
    const int logicalTopY = hoverInfo.anchorRect.top - 26;
    QVariantMap anchor = MakeCenteredAnchor(ToQString(hoverInfo.text),
        UiScaleLogicalToRawCoordinate(logicalCenterX),
        UiScaleLogicalToRawCoordinate(logicalTopY),
    QStringLiteral("#f0363b42"),
    QColor::fromRgba(hoverInfo.nameColorArgb).name(QColor::HexArgb));
    anchor.insert(QStringLiteral("kind"), QStringLiteral("uiTooltip"));
    return anchor;
}

bool TryAppendHoveredUiItemAnchor(QVariantList* anchors)
{
    if (!anchors) {
        return false;
    }

    shopui::ItemHoverInfo hoverInfo{};
    if (g_windowMgr.m_basicInfoWnd && g_windowMgr.m_basicInfoWnd->GetHoveredItemForQt(&hoverInfo) && hoverInfo.IsValid()) {
        anchors->push_back(MakeUiItemAnchor(hoverInfo));
        return true;
    }

    if (g_windowMgr.m_storageWnd && g_windowMgr.m_storageWnd->GetHoveredItemForQt(&hoverInfo) && hoverInfo.IsValid()) {
        anchors->push_back(MakeUiItemAnchor(hoverInfo));
        return true;
    }

    hoverInfo = shopui::ItemHoverInfo{};
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

void PopulateGuildMemberPresenceData(QVariantMap& row,const ro::guild::Member& member)
{
    row["online"]=member.online;
    row["gender"]=member.gender;
    row["hairStyle"]=member.hairStyle;
    row["hairColor"]=member.hairColor;
}

static void PopulateGuildWindow(QtUiState* state,CGameMode& mode)
{
    if(mode.m_pendingEmblemGuild && (!CRagConnection::instance()->IsOpen() || GetTickCount()-mode.m_pendingEmblemTick>=15000)) {
        g_windowMgr.PushChatEvent("Guild emblem update was not confirmed. Check the server message or reconnect and try again.",0x00FF4040u,6);
        DbgLog("[GuildEmblem] unconfirmed guild=%u connected=%d\n",mode.m_pendingEmblemGuild,CRagConnection::instance()->IsOpen());
        mode.m_pendingEmblemGuild=0;mode.m_pendingEmblemData.clear();
    }
    auto* window=g_windowMgr.m_guildWnd;
    QVariantMap data;data["visible"]=window && IsGameplayWindowVisible(state,window);
    if(!data["visible"].toBool()){state->setGuildWindow(data);return;}
    const auto& guild=g_session.m_localGuild;const auto& info=g_session.m_guildWindow;
    data["x"]=window->m_x;data["y"]=window->m_y;data["width"]=window->m_w;data["height"]=window->m_h;data["tab"]=window->tab;
    data["joined"]=guild.guildId!=0;data["guildId"]=guild.guildId;data["name"]=ToQString(guild.name);data["master"]=ToQString(info.master);data["isMaster"]=guild.isMaster;data["menuFlags"]=info.menuFlags;data["permissions"]=guild.permissions;
    data["loaded"]=info.loaded;data["level"]=info.level;data["online"]=info.online;data["capacity"]=info.capacity;data["averageLevel"]=info.averageLevel;
    data["exp"]=info.exp;data["nextExp"]=info.nextExp;data["territory"]=ToQString(info.territory);
    data["subject"]=ToQString(info.noticeTitle);data["notice"]=ToQString(info.notice);data["skillPoints"]=g_session.GetGuildSkillPoint();
    data["emblem"]=ResolveGuildEmblem(mode,mode.m_world?mode.m_world->m_player:nullptr);
    QVariantList members,positions,skills,relations,bans;
    for(const auto& m:info.members){
        QVariantMap row;row["name"]=ToQString(m.name);row["character"]=m.character;row["account"]=m.account;row["job"]=ToQString(g_session.GetJobDisplayName(m.job));
        row["level"]=m.level;row["exp"]=m.exp;row["position"]=m.position;PopulateGuildMemberPresenceData(row,m);
        const auto pos=info.positions.find(m.position);row["rank"]=pos==info.positions.end()?QString():ToQString(pos->second.name);members.push_back(row);
    }
    for(const auto& entry:info.positions){const auto& p=entry.second;QVariantMap row;row["id"]=p.id;row["name"]=ToQString(p.name);row["tax"]=p.tax;row["invite"]=(p.mode&1)!=0;row["expel"]=(p.mode&16)!=0;positions.push_back(row);}
    static const char* const skillNames[]={"Official Guild Approval","Kafra Contract","Guardian Research","Strengthen Guardians","Guild Extension","Guild's Glory","Great Leadership","Glorious Wounds","Cold Heart","Sharp Gaze","Battle Orders","Regeneration","Restoration","Urgent Call","Permanent Development","Item Emergency Call"};
    for(const auto& skill:g_session.GetGuildSkillItems()){
        if(!skill.m_isValid || (!skill.level && !skill.upgradable))continue;
        QVariantMap row;row["id"]=skill.SKID;row["name"]=skill.SKID>=10000&&skill.SKID<10016?ToQString(skillNames[skill.SKID-10000]):ToQString(skill.skillName);
        row["level"]=skill.level;row["upgrade"]=skill.upgradable!=0;skills.push_back(row);
    }
    for(const auto& r:info.relations){QVariantMap row;row["id"]=r.id;row["name"]=ToQString(r.name);row["hostile"]=r.hostile;relations.push_back(row);}
    for(const auto& ban:info.bans){QVariantMap row;row["name"]=ToQString(ban.name);row["reason"]=ToQString(ban.reason);bans.push_back(row);}
    data["members"]=members;data["positions"]=positions;data["skills"]=skills;data["relations"]=relations;data["bans"]=bans;
    state->setGuildWindow(data);
}

QtUiStateAdapter::QtUiStateAdapter()
    : m_state(new QtUiState())
{
    m_state->setLastInput(QStringLiteral("No routed input yet."));
    BindCommerceUi(m_state);
    QObject::connect(m_state,&QtUiState::guildActionRequested,[](const QString& action,const QVariantMap& args){
        auto* window=g_windowMgr.m_guildWnd;if(!window)return;
        if(action=="close"){window->SetShow(0);window->StoreInfo();return;}
        if(action=="move"){int x=args.value("x").toInt(),y=args.value("y").toInt();g_windowMgr.ClampWindowToClient(&x,&y,window->m_w,window->m_h);window->Move(x,y);return;}
        if(action=="tab"){
            const int tab=std::clamp(args.value("tab").toInt(),0,5);
            const u32 flags=g_session.m_guildWindow.menuFlags;
            const u32 required=tab==1?0x01u:tab==2?0x02u:tab==3?0x04u:tab==5?0x10u:0u;
            if(required==0 || (flags&required)!=0)window->tab=tab;
            return;
        }
        if(action=="refresh"){window->Refresh();return;}
        const auto& guild=g_session.m_localGuild;if(!guild.guildId)return;
        if(action=="emblem" && guild.isMaster){RequestGuildEmblemUpload();return;}
        std::vector<u8> packet;
        auto put=[&](int at,u32 value){for(int i=0;i<4;++i)packet[at+i]=static_cast<u8>(value>>(8*i));};
        auto begin=[&](u16 op,int size){packet.assign(size,0);packet[0]=static_cast<u8>(op);packet[1]=static_cast<u8>(op>>8);};
        auto text=[&](int at,int max,const QString& value){const auto bytes=value.toUtf8();if(bytes.size()>=max){g_windowMgr.PushChatEvent("Guild text is too long for the server. Please shorten it.",0x00FF4040u,6);return false;}std::memcpy(packet.data()+at,bytes.constData(),bytes.size());return true;};
        if(action=="notice" && guild.isMaster && (g_session.m_guildWindow.menuFlags&0x80u)){begin(0x16e,186);put(2,guild.guildId);if(!text(6,60,args.value("subject").toString())||!text(66,120,args.value("body").toString()))return;}
        else if(action=="position" && guild.isMaster && (g_session.m_guildWindow.menuFlags&0x02u)){
            const int id=args.value("id").toInt(),tax=args.value("tax").toInt();if(id<0||id>=20||tax<0||tax>50)return;
            begin(0x161,44);packet[2]=44;put(4,id);put(8,(args.value("invite").toBool()?1:0)|(args.value("expel").toBool()?16:0));put(12,id);put(16,tax);
            if(!text(20,24,args.value("name").toString()))return;
        } else if(action=="skill" && guild.isMaster && (g_session.m_guildWindow.menuFlags&0x04u) && g_session.GetGuildSkillPoint()>0){
            const int id=args.value("id").toInt();bool allowed=false;for(const auto& skill:g_session.GetGuildSkillItems())if(skill.SKID==id&&skill.upgradable)allowed=true;
            if(!allowed)return;begin(0x112,4);packet[2]=static_cast<u8>(id);packet[3]=static_cast<u8>(id>>8);
        } else if(action=="memberPosition" && guild.isMaster && (g_session.m_guildWindow.menuFlags&0x02u)){
            const u32 cid=args.value("character").toUInt(),position=args.value("position").toUInt();
            if(position>=20)return;
            const auto& members=g_session.m_guildWindow.members;const auto found=std::find_if(members.begin(),members.end(),[&](const auto& m){return m.character==cid;});
            if(found==members.end())return;begin(0x155,16);packet[2]=16;put(4,found->account);put(8,cid);put(12,position);
        } else if(action=="disband" && guild.isMaster){
            if (!args.value("confirmed").toBool() || args.value("guildId").toUInt() != guild.guildId
                || guild.name.empty() || args.value("name").toString() != ToQString(guild.name)) return;
            begin(0x15d, 42);
            // The server compares the key with the guild's original wire name.
            if (guild.name.size() >= 40) return;
            std::copy(guild.name.begin(), guild.name.end(), packet.begin() + 2);
        } else if(action=="expel" && (guild.isMaster || (guild.permissions & 0x10u))){
            const auto account = args.value("account").toUInt();
            const auto character = args.value("character").toUInt();
            const auto& info = g_session.m_guildWindow;
            if (!args.value("confirmed").toBool() || args.value("guildId").toUInt() != guild.guildId
                || !account || !character || character == g_session.m_gid) return;
            const auto member = std::find_if(info.members.begin(), info.members.end(), [&](const auto& entry) {
                return entry.account == account && entry.character == character;
            });
            if (member == info.members.end() || member->name == info.master) return;
            begin(0x15b, 54);put(2, guild.guildId);put(6, account);put(10, character);
            if (!text(14, 40, args.value("reason").toString())) return;
        } else if(action=="removeRelation" && guild.isMaster){
            const auto id = args.value("id").toUInt();
            const bool hostile = args.value("hostile").toBool();
            const auto& relations = g_session.m_guildWindow.relations;
            if (!id || std::none_of(relations.begin(), relations.end(), [&](const auto& relation) {
                return relation.id == id && relation.hostile == hostile;
            })) return;
            begin(0x183, 10);put(2, id);put(6, hostile ? 1 : 0);
        } else if(action=="leave" && !guild.isMaster && args.value("confirmed").toBool()){
            begin(0x159,54);put(2,guild.guildId);put(6,g_session.m_aid);put(10,g_session.m_gid);if(!text(14,40,args.value("reason").toString()))return;
        } else return;
        if(!CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),static_cast<int>(packet.size())))g_windowMgr.PushChatEvent("Unable to send guild request.",0x00FF4040u,6);
    });
    QObject::connect(m_state, &QtUiState::chatFocusRequested, [this](int field) {
        if(auto* chat=g_windowMgr.m_chatWnd)chat->FocusQtInput(field);
        syncChat();
    });
    QObject::connect(m_state, &QtUiState::chatEditRequested, [this](int field,const QString& text) {
        if(auto* chat=g_windowMgr.m_chatWnd)chat->SetQtInputText(field,text.toUtf8().toStdString());
        syncChat();
    });
    QObject::connect(m_state, &QtUiState::chatKeyRequested, [this](int key) {
        if(key==VK_ESCAPE && g_windowMgr.m_battleMsgOptionWnd
            && g_windowMgr.OnQtKeyDown(key,false,false,false)){syncChat();return;}
        if(auto* chat=g_windowMgr.m_chatWnd)chat->HandleKeyDown(key);
        syncChat();
    });
    QObject::connect(m_state, &QtUiState::closeCutinRequested, [] {
        if (CGameMode* const mode = g_modeMgr.GetCurrentGameMode()) {
            mode->SendMsg(113, reinterpret_cast<msgparam_t>(""), static_cast<msgparam_t>(-1), 0);
        }
    });
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

void QtUiStateAdapter::syncChat() { PopulateChatWindowState(m_state); }
void QtUiStateAdapter::syncMessenger() { PopulateFriendPartyState(m_state); }

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
    const QString wallpaperRevision = BuildWallpaperRevisionText();
    const QString renderPath = BuildRenderPathText(nativeOverlayBackend);
    const QString loginStatus = ToQString(g_windowMgr.GetLoginStatus());
    const QString chatPreview = BuildChatPreviewText();
    const QString lastInput = m_state->lastInput();

    m_state->setBackendName(backendName);
    m_state->setModeName(modeName);
    m_state->setWallpaperRevision(wallpaperRevision);
    m_state->setCutinInfo(QString(), -1);
    m_state->setRenderPath(renderPath);
    m_state->setArchitectureNote(BuildArchitectureNote(nativeOverlayBackend));
    m_state->setLoginStatus(loginStatus);
    m_state->setChatPreview(chatPreview);
    m_state->setUiScale(static_cast<double>(GetConfiguredUiScaleFactor()));
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
    ClearGameplayUiState(m_state);
    return true;
}

void QtUiStateAdapter::syncMinimap()
{
    PopulateMinimapState(m_state);
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
    const QString wallpaperRevision = BuildWallpaperRevisionText();
    const QString renderPath = BuildRenderPathText(nativeOverlayBackend);
    const QString loginStatus = ToQString(g_windowMgr.GetLoginStatus());
    const QString chatPreview = BuildChatPreviewText();
    const QString lastInput = m_state->lastInput();

    m_state->setBackendName(backendName);
    m_state->setModeName(modeName);
    m_state->setWallpaperRevision(wallpaperRevision);
    m_state->setCutinInfo(QString::fromLocal8Bit(mode.m_showImageInfo.name.c_str()), mode.m_showImageInfo.type);
    m_state->setRenderPath(renderPath);
    m_state->setArchitectureNote(BuildArchitectureNote(nativeOverlayBackend));
    m_state->setLoginStatus(loginStatus);
    m_state->setChatPreview(chatPreview);
    m_state->setUiScale(static_cast<double>(GetConfiguredUiScaleFactor()));
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
    PopulateCommerceUi(m_state,mode,IsGameplayUiSuppressed(m_state));
    PopulateStorageState(m_state);
    PopulateFriendPartyState(m_state);
    PopulateGuildWindow(m_state,mode);
    PopulatePartySetupState(m_state);
    PopulatePartyInviteState(m_state);
    PopulateEquipState(m_state);
    PopulateSkillListState(m_state);
    PopulateItemInfoState(m_state);
    PopulateSkillDescribeState(m_state);
    PopulateItemCollectionState(m_state);
    PopulateItemIdentifyState(m_state);
    PopulateSkillSelectState(m_state);
    PopulateItemCompositionState(m_state);
    PopulateOptionState(m_state);
    syncMinimap();
    const int uiMouseX = UiScaleRawToLogicalCoordinate(mouseX);
    const int uiMouseY = UiScaleRawToLogicalCoordinate(mouseY);
    PopulateStatusIconsState(m_state, uiMouseX, uiMouseY);
    PopulateShopChoiceState(m_state);
    PopulateNotificationState(m_state);

    QVariantList anchors;
    if (mode.m_world && mode.m_view) {
        const matrix& viewMatrix = mode.m_view->GetViewMatrix();
        const float cameraLongitude = mode.m_view->GetCameraLongitude();
        const u32 now = GetTickCount();

        auto appendPartyHpAnchor = [&](CGameActor* actor) {
            if (!actor || actor->m_isPc == 0 || !actor->m_isVisible) {
                return;
            }
            if (actor->m_gid == g_session.m_gid || actor->m_gid == g_session.m_aid) {
                return;
            }
            if (actor->m_stateId == kGameActorDeathStateId) {
                return;
            }

            const NamePair* const cachedEntry = FindCachedHoverActorNameEntry(mode, actor);
            if (!cachedEntry || cachedEntry->name.empty()) {
                SendActorNameRequest(mode, actor->m_gid);
                return;
            }

            const FRIEND_INFO* matchedMember = nullptr;
            for (const FRIEND_INFO& member : g_session.GetPartyList()) {
                if (member.characterName == cachedEntry->name) {
                    matchedMember = &member;
                    break;
                }
            }
            if (!matchedMember || matchedMember->partyMaxHp <= 0) {
                return;
            }

            int centerX = 0;
            int topY = 0;
            int labelY = 0;
            if (!mode.m_world->GetActorScreenMarker(viewMatrix, cameraLongitude, actor->m_gid, &centerX, &topY, &labelY)) {
                return;
            }

            const auto metrics=ResolvePlayerVitalsScreenMetrics(mode.m_view);
            anchors.push_back(MakePartyHpBarAnchor(centerX, labelY + metrics.topDelta, matchedMember->partyHp, matchedMember->partyMaxHp,metrics));
        };

        auto appendChatBubbleAnchor = [&](CGameActor* actor) {
            if (!actor || !actor->m_isVisible || !actor->HasActiveChatBubble(now)) {
                return;
            }

            int bubbleX = 0;
            int bubbleBottomY = 0;
            if (!mode.m_world->GetActorSpeechBubbleAnchor(viewMatrix, actor->m_gid, &bubbleX, &bubbleBottomY)) {
                return;
            }

            anchors.push_back(MakeSpeechBubbleAnchor(ToQString(actor->GetChatBubbleText()),
                bubbleX,
                bubbleBottomY));
        };

        appendChatBubbleAnchor(mode.m_world->m_player);
        for (const auto& runtimeEntry : mode.m_runtimeActors) {
            CGameActor* const actor = runtimeEntry.second;
            if (!actor || actor == mode.m_world->m_player) {
                continue;
            }
            appendPartyHpAnchor(actor);
            appendChatBubbleAnchor(actor);
        }
        for (const CSkill* skill : mode.m_world->m_skillList) {
            if (!skill || !skill->m_isVisible || !skill->HasActiveChatBubble(now)) {
                continue;
            }
            int bubbleX = 0;
            int bubbleBottomY = 0;
            if (mode.m_world->GetSkillSpeechBubbleAnchor(viewMatrix, skill->m_aid, &bubbleX, &bubbleBottomY)) {
                anchors.push_back(MakeSkillTalkboxAnchor(ToQString(skill->GetChatBubbleText()), bubbleX, bubbleBottomY));
            }
        }

        const bool hasUiItemHover = TryAppendHoveredUiItemAnchor(&anchors);
        const bool hasStatusIconHover = !hasUiItemHover && TryAppendHoveredStatusIconAnchor(m_state, uiMouseX, uiMouseY, &anchors);
        const bool blocksWorldHover = g_windowMgr.HasWindowAtPoint(uiMouseX, uiMouseY);
        const bool blocksGameplayHover = hasUiItemHover || hasStatusIconHover || blocksWorldHover;

        int labelX = 0;
        int labelY = 0;
        CGameActor* hoveredActor = nullptr;
        if (!blocksGameplayHover && mode.m_world->FindHoveredActorScreen(viewMatrix,
                cameraLongitude,
                mouseX,
                mouseY,
                &hoveredActor,
                &labelX,
                &labelY,true)) {
            if (!hoveredActor || hoveredActor->m_gid != mode.m_lastLockOnMonGid) {
                int anchorX = labelX;
                int anchorY = labelY + kQtActorLabelVerticalOffset;
                if (hoveredActor == mode.m_world->m_player || hoveredActor->m_gid == g_session.m_gid) {
                    mode.m_world->GetPlayerScreenLabel(viewMatrix, cameraLongitude, &anchorX, &anchorY);
                } else {
                    mode.m_world->GetActorScreenMarker(viewMatrix, cameraLongitude, hoveredActor->m_gid, &anchorX, nullptr, &anchorY);
                }
                // Owned companions draw the same zoom-scaled HP/SP overlay as the
                // local player, so their hover name must clear those bars too.
                if (hoveredActor->m_isPc || ActorHasPlayerStyleVitalsOverlay(hoveredActor)) {
                    anchorY += ResolvePlayerStyleVitalsNameOffsetY(mode.m_view);
                } else {
                    anchorY += kQtActorLabelVerticalOffset;
                }
                const QStringList lines=ToQString(ResolveActorLabel(mode,hoveredActor)).split('\n');
                auto nameplate=MakeCenteredAnchor(lines.value(0),
                    anchorX,
                    anchorY,
                    ResolveHoverBackground(hoveredActor),
                    ResolveHoverForeground(hoveredActor));
                nameplate.insert(QStringLiteral("centerX"),anchorX);
                if(lines.size()>1) {
                    nameplate.insert(QStringLiteral("guildText"),lines.mid(1).join('\n'));
                    nameplate.insert(QStringLiteral("emblemSource"),ResolveGuildEmblem(mode,hoveredActor));
                }
                anchors.push_back(nameplate);
            }
        } else if (!blocksGameplayHover) {
            CItem* hoveredItem = nullptr;
            if (mode.m_world->FindHoveredGroundItemScreen(viewMatrix,
                    mouseX,
                    mouseY,
                    &hoveredItem,
                    &labelX,
                    &labelY)) {
                QVariantMap anchor = MakeAnchor(ToQString(ResolveGroundItemLabel(hoveredItem)),
                    labelX, labelY, QStringLiteral("#c0087f5b"),
                    (hoveredItem && hoveredItem->m_identified != 0) ? QStringLiteral("#ffffff") : QStringLiteral("#b8b8b8"));
                // QML centers using the actual rendered text width and height.
                anchor.insert(QStringLiteral("centerX"), labelX);
                anchor.insert(QStringLiteral("bottomY"), labelY);
                anchors.push_back(anchor);
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
