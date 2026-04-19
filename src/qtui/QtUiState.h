#pragma once

#include <cmath>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

class QtUiState : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString backendName READ backendName NOTIFY backendNameChanged)
    Q_PROPERTY(QString modeName READ modeName NOTIFY modeNameChanged)
    Q_PROPERTY(QString wallpaperRevision READ wallpaperRevision NOTIFY wallpaperRevisionChanged)
    Q_PROPERTY(QString renderPath READ renderPath NOTIFY renderPathChanged)
    Q_PROPERTY(QString architectureNote READ architectureNote NOTIFY architectureNoteChanged)
    Q_PROPERTY(QString loginStatus READ loginStatus NOTIFY loginStatusChanged)
    Q_PROPERTY(QString chatPreview READ chatPreview NOTIFY chatPreviewChanged)
    Q_PROPERTY(QString lastInput READ lastInput NOTIFY lastInputChanged)
    Q_PROPERTY(double uiScale READ uiScale NOTIFY uiScaleChanged)
    Q_PROPERTY(QVariantMap debugOverlayData READ debugOverlayData NOTIFY debugOverlayDataChanged)
    Q_PROPERTY(bool serverSelectVisible READ serverSelectVisible NOTIFY serverSelectVisibleChanged)
    Q_PROPERTY(int serverPanelX READ serverPanelX NOTIFY serverPanelGeometryChanged)
    Q_PROPERTY(int serverPanelY READ serverPanelY NOTIFY serverPanelGeometryChanged)
    Q_PROPERTY(int serverPanelWidth READ serverPanelWidth NOTIFY serverPanelGeometryChanged)
    Q_PROPERTY(int serverPanelHeight READ serverPanelHeight NOTIFY serverPanelGeometryChanged)
    Q_PROPERTY(int serverSelectedIndex READ serverSelectedIndex NOTIFY serverSelectionChanged)
    Q_PROPERTY(int serverHoverIndex READ serverHoverIndex NOTIFY serverHoverIndexChanged)
    Q_PROPERTY(QVariantMap serverPanelData READ serverPanelData NOTIFY serverPanelDataChanged)
    Q_PROPERTY(QVariantList serverEntries READ serverEntries NOTIFY serverEntriesChanged)
    Q_PROPERTY(QStringList serverEntryLabels READ serverEntryLabels NOTIFY serverEntriesChanged)
    Q_PROPERTY(QStringList serverEntryDetails READ serverEntryDetails NOTIFY serverEntriesChanged)
    Q_PROPERTY(bool loginPanelVisible READ loginPanelVisible NOTIFY loginPanelVisibleChanged)
    Q_PROPERTY(int loginPanelX READ loginPanelX NOTIFY loginPanelGeometryChanged)
    Q_PROPERTY(int loginPanelY READ loginPanelY NOTIFY loginPanelGeometryChanged)
    Q_PROPERTY(int loginPanelWidth READ loginPanelWidth NOTIFY loginPanelGeometryChanged)
    Q_PROPERTY(int loginPanelHeight READ loginPanelHeight NOTIFY loginPanelGeometryChanged)
    Q_PROPERTY(QString loginUserId READ loginUserId NOTIFY loginPanelDataChanged)
    Q_PROPERTY(QString loginPasswordMask READ loginPasswordMask NOTIFY loginPanelDataChanged)
    Q_PROPERTY(bool loginSaveAccountChecked READ loginSaveAccountChecked NOTIFY loginPanelDataChanged)
    Q_PROPERTY(bool loginPasswordFocused READ loginPasswordFocused NOTIFY loginPanelDataChanged)
    Q_PROPERTY(QVariantMap loginPanelLabels READ loginPanelLabels NOTIFY loginPanelLabelsChanged)
    Q_PROPERTY(QVariantList loginButtons READ loginButtons NOTIFY loginButtonsChanged)
    Q_PROPERTY(bool charSelectVisible READ charSelectVisible NOTIFY charSelectVisibleChanged)
    Q_PROPERTY(int charSelectPanelX READ charSelectPanelX NOTIFY charSelectPanelGeometryChanged)
    Q_PROPERTY(int charSelectPanelY READ charSelectPanelY NOTIFY charSelectPanelGeometryChanged)
    Q_PROPERTY(int charSelectPanelWidth READ charSelectPanelWidth NOTIFY charSelectPanelGeometryChanged)
    Q_PROPERTY(int charSelectPanelHeight READ charSelectPanelHeight NOTIFY charSelectPanelGeometryChanged)
    Q_PROPERTY(int charSelectPage READ charSelectPage NOTIFY charSelectPageChanged)
    Q_PROPERTY(int charSelectPageCount READ charSelectPageCount NOTIFY charSelectPageChanged)
    Q_PROPERTY(QVariantList charSelectSlots READ charSelectSlots NOTIFY charSelectSlotsChanged)
    Q_PROPERTY(QVariantMap charSelectSelectedDetails READ charSelectSelectedDetails NOTIFY charSelectSelectedDetailsChanged)
    Q_PROPERTY(QVariantList charSelectPageButtons READ charSelectPageButtons NOTIFY charSelectPageButtonsChanged)
    Q_PROPERTY(QVariantList charSelectActionButtons READ charSelectActionButtons NOTIFY charSelectActionButtonsChanged)
    Q_PROPERTY(bool makeCharVisible READ makeCharVisible NOTIFY makeCharVisibleChanged)
    Q_PROPERTY(int makeCharPanelX READ makeCharPanelX NOTIFY makeCharPanelGeometryChanged)
    Q_PROPERTY(int makeCharPanelY READ makeCharPanelY NOTIFY makeCharPanelGeometryChanged)
    Q_PROPERTY(int makeCharPanelWidth READ makeCharPanelWidth NOTIFY makeCharPanelGeometryChanged)
    Q_PROPERTY(int makeCharPanelHeight READ makeCharPanelHeight NOTIFY makeCharPanelGeometryChanged)
    Q_PROPERTY(QString makeCharName READ makeCharName NOTIFY makeCharDataChanged)
    Q_PROPERTY(bool makeCharNameFocused READ makeCharNameFocused NOTIFY makeCharDataChanged)
    Q_PROPERTY(QVariantList makeCharStats READ makeCharStats NOTIFY makeCharDataChanged)
    Q_PROPERTY(int makeCharHairIndex READ makeCharHairIndex NOTIFY makeCharDataChanged)
    Q_PROPERTY(int makeCharHairColor READ makeCharHairColor NOTIFY makeCharDataChanged)
    Q_PROPERTY(QVariantMap makeCharPanelData READ makeCharPanelData NOTIFY makeCharPanelDataChanged)
    Q_PROPERTY(QVariantList makeCharButtons READ makeCharButtons NOTIFY makeCharButtonsChanged)
    Q_PROPERTY(QVariantList makeCharStatFields READ makeCharStatFields NOTIFY makeCharStatFieldsChanged)
    Q_PROPERTY(bool loadingVisible READ loadingVisible NOTIFY loadingVisibleChanged)
    Q_PROPERTY(QString loadingMessage READ loadingMessage NOTIFY loadingMessageChanged)
    Q_PROPERTY(double loadingProgress READ loadingProgress NOTIFY loadingProgressChanged)
    Q_PROPERTY(bool npcMenuVisible READ npcMenuVisible NOTIFY npcMenuVisibleChanged)
    Q_PROPERTY(int npcMenuX READ npcMenuX NOTIFY npcMenuGeometryChanged)
    Q_PROPERTY(int npcMenuY READ npcMenuY NOTIFY npcMenuGeometryChanged)
    Q_PROPERTY(int npcMenuWidth READ npcMenuWidth NOTIFY npcMenuGeometryChanged)
    Q_PROPERTY(int npcMenuHeight READ npcMenuHeight NOTIFY npcMenuGeometryChanged)
    Q_PROPERTY(int npcMenuSelectedIndex READ npcMenuSelectedIndex NOTIFY npcMenuSelectionChanged)
    Q_PROPERTY(int npcMenuHoverIndex READ npcMenuHoverIndex NOTIFY npcMenuHoverIndexChanged)
    Q_PROPERTY(bool npcMenuOkPressed READ npcMenuOkPressed NOTIFY npcMenuButtonsChanged)
    Q_PROPERTY(bool npcMenuCancelPressed READ npcMenuCancelPressed NOTIFY npcMenuButtonsChanged)
    Q_PROPERTY(QVariantList npcMenuOptions READ npcMenuOptions NOTIFY npcMenuOptionsChanged)
    Q_PROPERTY(QVariantList npcMenuButtons READ npcMenuButtons NOTIFY npcMenuButtonsChanged)
    Q_PROPERTY(bool sayDialogVisible READ sayDialogVisible NOTIFY sayDialogVisibleChanged)
    Q_PROPERTY(int sayDialogX READ sayDialogX NOTIFY sayDialogGeometryChanged)
    Q_PROPERTY(int sayDialogY READ sayDialogY NOTIFY sayDialogGeometryChanged)
    Q_PROPERTY(int sayDialogWidth READ sayDialogWidth NOTIFY sayDialogGeometryChanged)
    Q_PROPERTY(int sayDialogHeight READ sayDialogHeight NOTIFY sayDialogGeometryChanged)
    Q_PROPERTY(QString sayDialogText READ sayDialogText NOTIFY sayDialogTextChanged)
    Q_PROPERTY(bool sayDialogHasAction READ sayDialogHasAction NOTIFY sayDialogActionChanged)
    Q_PROPERTY(QString sayDialogActionLabel READ sayDialogActionLabel NOTIFY sayDialogActionChanged)
    Q_PROPERTY(bool sayDialogActionHovered READ sayDialogActionHovered NOTIFY sayDialogActionChanged)
    Q_PROPERTY(bool sayDialogActionPressed READ sayDialogActionPressed NOTIFY sayDialogActionChanged)
    Q_PROPERTY(QVariantMap sayDialogActionButton READ sayDialogActionButton NOTIFY sayDialogActionChanged)
    Q_PROPERTY(bool npcInputVisible READ npcInputVisible NOTIFY npcInputVisibleChanged)
    Q_PROPERTY(int npcInputX READ npcInputX NOTIFY npcInputGeometryChanged)
    Q_PROPERTY(int npcInputY READ npcInputY NOTIFY npcInputGeometryChanged)
    Q_PROPERTY(int npcInputWidth READ npcInputWidth NOTIFY npcInputGeometryChanged)
    Q_PROPERTY(int npcInputHeight READ npcInputHeight NOTIFY npcInputGeometryChanged)
    Q_PROPERTY(QString npcInputLabel READ npcInputLabel NOTIFY npcInputTextChanged)
    Q_PROPERTY(QString npcInputText READ npcInputText NOTIFY npcInputTextChanged)
    Q_PROPERTY(bool npcInputOkPressed READ npcInputOkPressed NOTIFY npcInputButtonsChanged)
    Q_PROPERTY(bool npcInputCancelPressed READ npcInputCancelPressed NOTIFY npcInputButtonsChanged)
    Q_PROPERTY(QVariantList npcInputButtons READ npcInputButtons NOTIFY npcInputButtonsChanged)
    Q_PROPERTY(bool chooseMenuVisible READ chooseMenuVisible NOTIFY chooseMenuVisibleChanged)
    Q_PROPERTY(int chooseMenuX READ chooseMenuX NOTIFY chooseMenuGeometryChanged)
    Q_PROPERTY(int chooseMenuY READ chooseMenuY NOTIFY chooseMenuGeometryChanged)
    Q_PROPERTY(int chooseMenuWidth READ chooseMenuWidth NOTIFY chooseMenuGeometryChanged)
    Q_PROPERTY(int chooseMenuHeight READ chooseMenuHeight NOTIFY chooseMenuGeometryChanged)
    Q_PROPERTY(int chooseMenuSelectedIndex READ chooseMenuSelectedIndex NOTIFY chooseMenuSelectedIndexChanged)
    Q_PROPERTY(int chooseMenuPressedIndex READ chooseMenuPressedIndex NOTIFY chooseMenuPressedIndexChanged)
    Q_PROPERTY(QVariantList chooseMenuOptions READ chooseMenuOptions NOTIFY chooseMenuOptionsChanged)
    Q_PROPERTY(bool itemShopVisible READ itemShopVisible NOTIFY itemShopVisibleChanged)
    Q_PROPERTY(int itemShopX READ itemShopX NOTIFY itemShopGeometryChanged)
    Q_PROPERTY(int itemShopY READ itemShopY NOTIFY itemShopGeometryChanged)
    Q_PROPERTY(int itemShopWidth READ itemShopWidth NOTIFY itemShopGeometryChanged)
    Q_PROPERTY(int itemShopHeight READ itemShopHeight NOTIFY itemShopGeometryChanged)
    Q_PROPERTY(QString itemShopTitle READ itemShopTitle NOTIFY itemShopTitleChanged)
    Q_PROPERTY(QVariantMap itemShopData READ itemShopData NOTIFY itemShopDataChanged)
    Q_PROPERTY(QVariantList itemShopRows READ itemShopRows NOTIFY itemShopRowsChanged)
    Q_PROPERTY(bool itemPurchaseVisible READ itemPurchaseVisible NOTIFY itemPurchaseVisibleChanged)
    Q_PROPERTY(int itemPurchaseX READ itemPurchaseX NOTIFY itemPurchaseGeometryChanged)
    Q_PROPERTY(int itemPurchaseY READ itemPurchaseY NOTIFY itemPurchaseGeometryChanged)
    Q_PROPERTY(int itemPurchaseWidth READ itemPurchaseWidth NOTIFY itemPurchaseGeometryChanged)
    Q_PROPERTY(int itemPurchaseHeight READ itemPurchaseHeight NOTIFY itemPurchaseGeometryChanged)
    Q_PROPERTY(int itemPurchaseTotal READ itemPurchaseTotal NOTIFY itemPurchaseTotalChanged)
    Q_PROPERTY(QVariantMap itemPurchaseData READ itemPurchaseData NOTIFY itemPurchaseDataChanged)
    Q_PROPERTY(QVariantList itemPurchaseRows READ itemPurchaseRows NOTIFY itemPurchaseRowsChanged)
    Q_PROPERTY(QVariantList itemPurchaseButtons READ itemPurchaseButtons NOTIFY itemPurchaseButtonsChanged)
    Q_PROPERTY(bool itemSellVisible READ itemSellVisible NOTIFY itemSellVisibleChanged)
    Q_PROPERTY(int itemSellX READ itemSellX NOTIFY itemSellGeometryChanged)
    Q_PROPERTY(int itemSellY READ itemSellY NOTIFY itemSellGeometryChanged)
    Q_PROPERTY(int itemSellWidth READ itemSellWidth NOTIFY itemSellGeometryChanged)
    Q_PROPERTY(int itemSellHeight READ itemSellHeight NOTIFY itemSellGeometryChanged)
    Q_PROPERTY(int itemSellTotal READ itemSellTotal NOTIFY itemSellTotalChanged)
    Q_PROPERTY(QVariantMap itemSellData READ itemSellData NOTIFY itemSellDataChanged)
    Q_PROPERTY(QVariantList itemSellRows READ itemSellRows NOTIFY itemSellRowsChanged)
    Q_PROPERTY(QVariantList itemSellButtons READ itemSellButtons NOTIFY itemSellButtonsChanged)
    Q_PROPERTY(bool shortCutVisible READ shortCutVisible NOTIFY shortCutVisibleChanged)
    Q_PROPERTY(int shortCutX READ shortCutX NOTIFY shortCutGeometryChanged)
    Q_PROPERTY(int shortCutY READ shortCutY NOTIFY shortCutGeometryChanged)
    Q_PROPERTY(int shortCutWidth READ shortCutWidth NOTIFY shortCutGeometryChanged)
    Q_PROPERTY(int shortCutHeight READ shortCutHeight NOTIFY shortCutGeometryChanged)
    Q_PROPERTY(int shortCutPage READ shortCutPage NOTIFY shortCutPageChanged)
    Q_PROPERTY(int shortCutHoverSlot READ shortCutHoverSlot NOTIFY shortCutHoverSlotChanged)
    Q_PROPERTY(QVariantList shortCutSlots READ shortCutSlots NOTIFY shortCutSlotsChanged)
    Q_PROPERTY(bool basicInfoVisible READ basicInfoVisible NOTIFY basicInfoVisibleChanged)
    Q_PROPERTY(int basicInfoX READ basicInfoX NOTIFY basicInfoGeometryChanged)
    Q_PROPERTY(int basicInfoY READ basicInfoY NOTIFY basicInfoGeometryChanged)
    Q_PROPERTY(int basicInfoWidth READ basicInfoWidth NOTIFY basicInfoGeometryChanged)
    Q_PROPERTY(int basicInfoHeight READ basicInfoHeight NOTIFY basicInfoGeometryChanged)
    Q_PROPERTY(bool basicInfoMini READ basicInfoMini NOTIFY basicInfoMiniChanged)
    Q_PROPERTY(QVariantMap basicInfoData READ basicInfoData NOTIFY basicInfoDataChanged)
    Q_PROPERTY(bool statusVisible READ statusVisible NOTIFY statusVisibleChanged)
    Q_PROPERTY(int statusX READ statusX NOTIFY statusGeometryChanged)
    Q_PROPERTY(int statusY READ statusY NOTIFY statusGeometryChanged)
    Q_PROPERTY(int statusWidth READ statusWidth NOTIFY statusGeometryChanged)
    Q_PROPERTY(int statusHeight READ statusHeight NOTIFY statusGeometryChanged)
    Q_PROPERTY(bool statusMini READ statusMini NOTIFY statusMiniChanged)
    Q_PROPERTY(int statusPage READ statusPage NOTIFY statusPageChanged)
    Q_PROPERTY(QVariantMap statusData READ statusData NOTIFY statusDataChanged)
    Q_PROPERTY(bool chatWindowVisible READ chatWindowVisible NOTIFY chatWindowVisibleChanged)
    Q_PROPERTY(int chatWindowX READ chatWindowX NOTIFY chatWindowGeometryChanged)
    Q_PROPERTY(int chatWindowY READ chatWindowY NOTIFY chatWindowGeometryChanged)
    Q_PROPERTY(int chatWindowWidth READ chatWindowWidth NOTIFY chatWindowGeometryChanged)
    Q_PROPERTY(int chatWindowHeight READ chatWindowHeight NOTIFY chatWindowGeometryChanged)
    Q_PROPERTY(bool chatWindowInputActive READ chatWindowInputActive NOTIFY chatWindowInputActiveChanged)
    Q_PROPERTY(bool chatWindowWhisperInputActive READ chatWindowWhisperInputActive NOTIFY chatWindowWhisperInputActiveChanged)
    Q_PROPERTY(bool chatWindowMessageInputActive READ chatWindowMessageInputActive NOTIFY chatWindowMessageInputActiveChanged)
    Q_PROPERTY(QString chatWindowWhisperTargetText READ chatWindowWhisperTargetText NOTIFY chatWindowWhisperTargetTextChanged)
    Q_PROPERTY(QString chatWindowInputText READ chatWindowInputText NOTIFY chatWindowInputTextChanged)
    Q_PROPERTY(QVariantList chatWindowLines READ chatWindowLines NOTIFY chatWindowLinesChanged)
    Q_PROPERTY(QVariantMap chatWindowScrollBar READ chatWindowScrollBar NOTIFY chatWindowScrollBarChanged)
    Q_PROPERTY(QVariantMap chatWindowUi READ chatWindowUi NOTIFY chatWindowUiChanged)
    Q_PROPERTY(bool rechargeGaugeVisible READ rechargeGaugeVisible NOTIFY rechargeGaugeVisibleChanged)
    Q_PROPERTY(int rechargeGaugeX READ rechargeGaugeX NOTIFY rechargeGaugeGeometryChanged)
    Q_PROPERTY(int rechargeGaugeY READ rechargeGaugeY NOTIFY rechargeGaugeGeometryChanged)
    Q_PROPERTY(int rechargeGaugeWidth READ rechargeGaugeWidth NOTIFY rechargeGaugeGeometryChanged)
    Q_PROPERTY(int rechargeGaugeHeight READ rechargeGaugeHeight NOTIFY rechargeGaugeGeometryChanged)
    Q_PROPERTY(int rechargeGaugeAmount READ rechargeGaugeAmount NOTIFY rechargeGaugeProgressChanged)
    Q_PROPERTY(int rechargeGaugeTotal READ rechargeGaugeTotal NOTIFY rechargeGaugeProgressChanged)
    Q_PROPERTY(bool inventoryVisible READ inventoryVisible NOTIFY inventoryVisibleChanged)
    Q_PROPERTY(int inventoryX READ inventoryX NOTIFY inventoryGeometryChanged)
    Q_PROPERTY(int inventoryY READ inventoryY NOTIFY inventoryGeometryChanged)
    Q_PROPERTY(int inventoryWidth READ inventoryWidth NOTIFY inventoryGeometryChanged)
    Q_PROPERTY(int inventoryHeight READ inventoryHeight NOTIFY inventoryGeometryChanged)
    Q_PROPERTY(bool inventoryMini READ inventoryMini NOTIFY inventoryMiniChanged)
    Q_PROPERTY(int inventoryTab READ inventoryTab NOTIFY inventoryTabChanged)
    Q_PROPERTY(QVariantMap inventoryData READ inventoryData NOTIFY inventoryDataChanged)
    Q_PROPERTY(bool storageVisible READ storageVisible NOTIFY storageVisibleChanged)
    Q_PROPERTY(int storageX READ storageX NOTIFY storageGeometryChanged)
    Q_PROPERTY(int storageY READ storageY NOTIFY storageGeometryChanged)
    Q_PROPERTY(int storageWidth READ storageWidth NOTIFY storageGeometryChanged)
    Q_PROPERTY(int storageHeight READ storageHeight NOTIFY storageGeometryChanged)
    Q_PROPERTY(bool storageMini READ storageMini NOTIFY storageMiniChanged)
    Q_PROPERTY(int storageTab READ storageTab NOTIFY storageTabChanged)
    Q_PROPERTY(QVariantMap storageData READ storageData NOTIFY storageDataChanged)
    Q_PROPERTY(bool equipVisible READ equipVisible NOTIFY equipVisibleChanged)
    Q_PROPERTY(int equipX READ equipX NOTIFY equipGeometryChanged)
    Q_PROPERTY(int equipY READ equipY NOTIFY equipGeometryChanged)
    Q_PROPERTY(int equipWidth READ equipWidth NOTIFY equipGeometryChanged)
    Q_PROPERTY(int equipHeight READ equipHeight NOTIFY equipGeometryChanged)
    Q_PROPERTY(bool equipMini READ equipMini NOTIFY equipMiniChanged)
    Q_PROPERTY(QVariantMap equipData READ equipData NOTIFY equipDataChanged)
    Q_PROPERTY(bool skillListVisible READ skillListVisible NOTIFY skillListVisibleChanged)
    Q_PROPERTY(int skillListX READ skillListX NOTIFY skillListGeometryChanged)
    Q_PROPERTY(int skillListY READ skillListY NOTIFY skillListGeometryChanged)
    Q_PROPERTY(int skillListWidth READ skillListWidth NOTIFY skillListGeometryChanged)
    Q_PROPERTY(int skillListHeight READ skillListHeight NOTIFY skillListGeometryChanged)
    Q_PROPERTY(QVariantMap skillListData READ skillListData NOTIFY skillListDataChanged)
    Q_PROPERTY(bool itemInfoVisible READ itemInfoVisible NOTIFY itemInfoVisibleChanged)
    Q_PROPERTY(int itemInfoX READ itemInfoX NOTIFY itemInfoGeometryChanged)
    Q_PROPERTY(int itemInfoY READ itemInfoY NOTIFY itemInfoGeometryChanged)
    Q_PROPERTY(int itemInfoWidth READ itemInfoWidth NOTIFY itemInfoGeometryChanged)
    Q_PROPERTY(int itemInfoHeight READ itemInfoHeight NOTIFY itemInfoGeometryChanged)
    Q_PROPERTY(QVariantMap itemInfoData READ itemInfoData NOTIFY itemInfoDataChanged)
    Q_PROPERTY(bool skillDescribeVisible READ skillDescribeVisible NOTIFY skillDescribeVisibleChanged)
    Q_PROPERTY(int skillDescribeX READ skillDescribeX NOTIFY skillDescribeGeometryChanged)
    Q_PROPERTY(int skillDescribeY READ skillDescribeY NOTIFY skillDescribeGeometryChanged)
    Q_PROPERTY(int skillDescribeWidth READ skillDescribeWidth NOTIFY skillDescribeGeometryChanged)
    Q_PROPERTY(int skillDescribeHeight READ skillDescribeHeight NOTIFY skillDescribeGeometryChanged)
    Q_PROPERTY(QVariantMap skillDescribeData READ skillDescribeData NOTIFY skillDescribeDataChanged)
    Q_PROPERTY(bool itemCollectionVisible READ itemCollectionVisible NOTIFY itemCollectionVisibleChanged)
    Q_PROPERTY(int itemCollectionX READ itemCollectionX NOTIFY itemCollectionGeometryChanged)
    Q_PROPERTY(int itemCollectionY READ itemCollectionY NOTIFY itemCollectionGeometryChanged)
    Q_PROPERTY(int itemCollectionWidth READ itemCollectionWidth NOTIFY itemCollectionGeometryChanged)
    Q_PROPERTY(int itemCollectionHeight READ itemCollectionHeight NOTIFY itemCollectionGeometryChanged)
    Q_PROPERTY(QVariantMap itemCollectionData READ itemCollectionData NOTIFY itemCollectionDataChanged)
    Q_PROPERTY(bool optionVisible READ optionVisible NOTIFY optionVisibleChanged)
    Q_PROPERTY(int optionX READ optionX NOTIFY optionGeometryChanged)
    Q_PROPERTY(int optionY READ optionY NOTIFY optionGeometryChanged)
    Q_PROPERTY(int optionWidth READ optionWidth NOTIFY optionGeometryChanged)
    Q_PROPERTY(int optionHeight READ optionHeight NOTIFY optionGeometryChanged)
    Q_PROPERTY(QVariantMap optionData READ optionData NOTIFY optionDataChanged)
    Q_PROPERTY(bool minimapVisible READ minimapVisible NOTIFY minimapVisibleChanged)
    Q_PROPERTY(int minimapX READ minimapX NOTIFY minimapGeometryChanged)
    Q_PROPERTY(int minimapY READ minimapY NOTIFY minimapGeometryChanged)
    Q_PROPERTY(int minimapWidth READ minimapWidth NOTIFY minimapGeometryChanged)
    Q_PROPERTY(int minimapHeight READ minimapHeight NOTIFY minimapGeometryChanged)
    Q_PROPERTY(QVariantMap minimapData READ minimapData NOTIFY minimapDataChanged)
    Q_PROPERTY(QVariantList statusIcons READ statusIcons NOTIFY statusIconsChanged)
    Q_PROPERTY(bool shopChoiceVisible READ shopChoiceVisible NOTIFY shopChoiceVisibleChanged)
    Q_PROPERTY(int shopChoiceX READ shopChoiceX NOTIFY shopChoiceGeometryChanged)
    Q_PROPERTY(int shopChoiceY READ shopChoiceY NOTIFY shopChoiceGeometryChanged)
    Q_PROPERTY(int shopChoiceWidth READ shopChoiceWidth NOTIFY shopChoiceGeometryChanged)
    Q_PROPERTY(int shopChoiceHeight READ shopChoiceHeight NOTIFY shopChoiceGeometryChanged)
    Q_PROPERTY(QString shopChoiceTitle READ shopChoiceTitle NOTIFY shopChoiceTextChanged)
    Q_PROPERTY(QString shopChoicePrompt READ shopChoicePrompt NOTIFY shopChoiceTextChanged)
    Q_PROPERTY(QVariantList shopChoiceButtons READ shopChoiceButtons NOTIFY shopChoiceButtonsChanged)
    Q_PROPERTY(QVariantList notifications READ notifications NOTIFY notificationsChanged)
    Q_PROPERTY(QVariantList anchors READ anchors NOTIFY anchorsChanged)

public:
    explicit QtUiState(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    const QString& backendName() const { return m_backendName; }
    const QString& modeName() const { return m_modeName; }
    const QString& wallpaperRevision() const { return m_wallpaperRevision; }
    const QString& renderPath() const { return m_renderPath; }
    const QString& architectureNote() const { return m_architectureNote; }
    const QString& loginStatus() const { return m_loginStatus; }
    const QString& chatPreview() const { return m_chatPreview; }
    const QString& lastInput() const { return m_lastInput; }
    double uiScale() const { return m_uiScale; }
    const QVariantMap& debugOverlayData() const { return m_debugOverlayData; }
    bool serverSelectVisible() const { return m_serverSelectVisible; }
    int serverPanelX() const { return m_serverPanelX; }
    int serverPanelY() const { return m_serverPanelY; }
    int serverPanelWidth() const { return m_serverPanelWidth; }
    int serverPanelHeight() const { return m_serverPanelHeight; }
    int serverSelectedIndex() const { return m_serverSelectedIndex; }
    int serverHoverIndex() const { return m_serverHoverIndex; }
    const QVariantMap& serverPanelData() const { return m_serverPanelData; }
    const QVariantList& serverEntries() const { return m_serverEntries; }
    const QStringList& serverEntryLabels() const { return m_serverEntryLabels; }
    const QStringList& serverEntryDetails() const { return m_serverEntryDetails; }
    bool loginPanelVisible() const { return m_loginPanelVisible; }
    int loginPanelX() const { return m_loginPanelX; }
    int loginPanelY() const { return m_loginPanelY; }
    int loginPanelWidth() const { return m_loginPanelWidth; }
    int loginPanelHeight() const { return m_loginPanelHeight; }
    const QString& loginUserId() const { return m_loginUserId; }
    const QString& loginPasswordMask() const { return m_loginPasswordMask; }
    bool loginSaveAccountChecked() const { return m_loginSaveAccountChecked; }
    bool loginPasswordFocused() const { return m_loginPasswordFocused; }
    const QVariantMap& loginPanelLabels() const { return m_loginPanelLabels; }
    const QVariantList& loginButtons() const { return m_loginButtons; }
    bool charSelectVisible() const { return m_charSelectVisible; }
    int charSelectPanelX() const { return m_charSelectPanelX; }
    int charSelectPanelY() const { return m_charSelectPanelY; }
    int charSelectPanelWidth() const { return m_charSelectPanelWidth; }
    int charSelectPanelHeight() const { return m_charSelectPanelHeight; }
    int charSelectPage() const { return m_charSelectPage; }
    int charSelectPageCount() const { return m_charSelectPageCount; }
    const QVariantList& charSelectSlots() const { return m_charSelectSlots; }
    const QVariantMap& charSelectSelectedDetails() const { return m_charSelectSelectedDetails; }
    const QVariantList& charSelectPageButtons() const { return m_charSelectPageButtons; }
    const QVariantList& charSelectActionButtons() const { return m_charSelectActionButtons; }
    bool makeCharVisible() const { return m_makeCharVisible; }
    int makeCharPanelX() const { return m_makeCharPanelX; }
    int makeCharPanelY() const { return m_makeCharPanelY; }
    int makeCharPanelWidth() const { return m_makeCharPanelWidth; }
    int makeCharPanelHeight() const { return m_makeCharPanelHeight; }
    const QString& makeCharName() const { return m_makeCharName; }
    bool makeCharNameFocused() const { return m_makeCharNameFocused; }
    const QVariantList& makeCharStats() const { return m_makeCharStats; }
    int makeCharHairIndex() const { return m_makeCharHairIndex; }
    int makeCharHairColor() const { return m_makeCharHairColor; }
    const QVariantMap& makeCharPanelData() const { return m_makeCharPanelData; }
    const QVariantList& makeCharButtons() const { return m_makeCharButtons; }
    const QVariantList& makeCharStatFields() const { return m_makeCharStatFields; }
    bool loadingVisible() const { return m_loadingVisible; }
    const QString& loadingMessage() const { return m_loadingMessage; }
    double loadingProgress() const { return m_loadingProgress; }
    bool npcMenuVisible() const { return m_npcMenuVisible; }
    int npcMenuX() const { return m_npcMenuX; }
    int npcMenuY() const { return m_npcMenuY; }
    int npcMenuWidth() const { return m_npcMenuWidth; }
    int npcMenuHeight() const { return m_npcMenuHeight; }
    int npcMenuSelectedIndex() const { return m_npcMenuSelectedIndex; }
    int npcMenuHoverIndex() const { return m_npcMenuHoverIndex; }
    bool npcMenuOkPressed() const { return m_npcMenuOkPressed; }
    bool npcMenuCancelPressed() const { return m_npcMenuCancelPressed; }
    const QVariantList& npcMenuOptions() const { return m_npcMenuOptions; }
    const QVariantList& npcMenuButtons() const { return m_npcMenuButtons; }
    bool sayDialogVisible() const { return m_sayDialogVisible; }
    int sayDialogX() const { return m_sayDialogX; }
    int sayDialogY() const { return m_sayDialogY; }
    int sayDialogWidth() const { return m_sayDialogWidth; }
    int sayDialogHeight() const { return m_sayDialogHeight; }
    const QString& sayDialogText() const { return m_sayDialogText; }
    bool sayDialogHasAction() const { return m_sayDialogHasAction; }
    const QString& sayDialogActionLabel() const { return m_sayDialogActionLabel; }
    bool sayDialogActionHovered() const { return m_sayDialogActionHovered; }
    bool sayDialogActionPressed() const { return m_sayDialogActionPressed; }
    const QVariantMap& sayDialogActionButton() const { return m_sayDialogActionButton; }
    bool npcInputVisible() const { return m_npcInputVisible; }
    int npcInputX() const { return m_npcInputX; }
    int npcInputY() const { return m_npcInputY; }
    int npcInputWidth() const { return m_npcInputWidth; }
    int npcInputHeight() const { return m_npcInputHeight; }
    const QString& npcInputLabel() const { return m_npcInputLabel; }
    const QString& npcInputText() const { return m_npcInputText; }
    bool npcInputOkPressed() const { return m_npcInputOkPressed; }
    bool npcInputCancelPressed() const { return m_npcInputCancelPressed; }
    const QVariantList& npcInputButtons() const { return m_npcInputButtons; }
    bool chooseMenuVisible() const { return m_chooseMenuVisible; }
    int chooseMenuX() const { return m_chooseMenuX; }
    int chooseMenuY() const { return m_chooseMenuY; }
    int chooseMenuWidth() const { return m_chooseMenuWidth; }
    int chooseMenuHeight() const { return m_chooseMenuHeight; }
    int chooseMenuSelectedIndex() const { return m_chooseMenuSelectedIndex; }
    int chooseMenuPressedIndex() const { return m_chooseMenuPressedIndex; }
    const QVariantList& chooseMenuOptions() const { return m_chooseMenuOptions; }
    bool itemShopVisible() const { return m_itemShopVisible; }
    int itemShopX() const { return m_itemShopX; }
    int itemShopY() const { return m_itemShopY; }
    int itemShopWidth() const { return m_itemShopWidth; }
    int itemShopHeight() const { return m_itemShopHeight; }
    const QString& itemShopTitle() const { return m_itemShopTitle; }
    const QVariantMap& itemShopData() const { return m_itemShopData; }
    const QVariantList& itemShopRows() const { return m_itemShopRows; }
    bool itemPurchaseVisible() const { return m_itemPurchaseVisible; }
    int itemPurchaseX() const { return m_itemPurchaseX; }
    int itemPurchaseY() const { return m_itemPurchaseY; }
    int itemPurchaseWidth() const { return m_itemPurchaseWidth; }
    int itemPurchaseHeight() const { return m_itemPurchaseHeight; }
    int itemPurchaseTotal() const { return m_itemPurchaseTotal; }
    const QVariantMap& itemPurchaseData() const { return m_itemPurchaseData; }
    const QVariantList& itemPurchaseRows() const { return m_itemPurchaseRows; }
    const QVariantList& itemPurchaseButtons() const { return m_itemPurchaseButtons; }
    bool itemSellVisible() const { return m_itemSellVisible; }
    int itemSellX() const { return m_itemSellX; }
    int itemSellY() const { return m_itemSellY; }
    int itemSellWidth() const { return m_itemSellWidth; }
    int itemSellHeight() const { return m_itemSellHeight; }
    int itemSellTotal() const { return m_itemSellTotal; }
    const QVariantMap& itemSellData() const { return m_itemSellData; }
    const QVariantList& itemSellRows() const { return m_itemSellRows; }
    const QVariantList& itemSellButtons() const { return m_itemSellButtons; }
    bool shortCutVisible() const { return m_shortCutVisible; }
    int shortCutX() const { return m_shortCutX; }
    int shortCutY() const { return m_shortCutY; }
    int shortCutWidth() const { return m_shortCutWidth; }
    int shortCutHeight() const { return m_shortCutHeight; }
    int shortCutPage() const { return m_shortCutPage; }
    int shortCutHoverSlot() const { return m_shortCutHoverSlot; }
    const QVariantList& shortCutSlots() const { return m_shortCutSlots; }
    bool basicInfoVisible() const { return m_basicInfoVisible; }
    int basicInfoX() const { return m_basicInfoX; }
    int basicInfoY() const { return m_basicInfoY; }
    int basicInfoWidth() const { return m_basicInfoWidth; }
    int basicInfoHeight() const { return m_basicInfoHeight; }
    bool basicInfoMini() const { return m_basicInfoMini; }
    const QVariantMap& basicInfoData() const { return m_basicInfoData; }
    bool statusVisible() const { return m_statusVisible; }
    int statusX() const { return m_statusX; }
    int statusY() const { return m_statusY; }
    int statusWidth() const { return m_statusWidth; }
    int statusHeight() const { return m_statusHeight; }
    bool statusMini() const { return m_statusMini; }
    int statusPage() const { return m_statusPage; }
    const QVariantMap& statusData() const { return m_statusData; }
    bool chatWindowVisible() const { return m_chatWindowVisible; }
    int chatWindowX() const { return m_chatWindowX; }
    int chatWindowY() const { return m_chatWindowY; }
    int chatWindowWidth() const { return m_chatWindowWidth; }
    int chatWindowHeight() const { return m_chatWindowHeight; }
    bool chatWindowInputActive() const { return m_chatWindowInputActive; }
    bool chatWindowWhisperInputActive() const { return m_chatWindowWhisperInputActive; }
    bool chatWindowMessageInputActive() const { return m_chatWindowMessageInputActive; }
    const QString& chatWindowWhisperTargetText() const { return m_chatWindowWhisperTargetText; }
    const QString& chatWindowInputText() const { return m_chatWindowInputText; }
    const QVariantList& chatWindowLines() const { return m_chatWindowLines; }
    const QVariantMap& chatWindowScrollBar() const { return m_chatWindowScrollBar; }
    const QVariantMap& chatWindowUi() const { return m_chatWindowUi; }
    bool rechargeGaugeVisible() const { return m_rechargeGaugeVisible; }
    int rechargeGaugeX() const { return m_rechargeGaugeX; }
    int rechargeGaugeY() const { return m_rechargeGaugeY; }
    int rechargeGaugeWidth() const { return m_rechargeGaugeWidth; }
    int rechargeGaugeHeight() const { return m_rechargeGaugeHeight; }
    int rechargeGaugeAmount() const { return m_rechargeGaugeAmount; }
    int rechargeGaugeTotal() const { return m_rechargeGaugeTotal; }
    bool inventoryVisible() const { return m_inventoryVisible; }
    int inventoryX() const { return m_inventoryX; }
    int inventoryY() const { return m_inventoryY; }
    int inventoryWidth() const { return m_inventoryWidth; }
    int inventoryHeight() const { return m_inventoryHeight; }
    bool inventoryMini() const { return m_inventoryMini; }
    int inventoryTab() const { return m_inventoryTab; }
    const QVariantMap& inventoryData() const { return m_inventoryData; }
    bool storageVisible() const { return m_storageVisible; }
    int storageX() const { return m_storageX; }
    int storageY() const { return m_storageY; }
    int storageWidth() const { return m_storageWidth; }
    int storageHeight() const { return m_storageHeight; }
    bool storageMini() const { return m_storageMini; }
    int storageTab() const { return m_storageTab; }
    const QVariantMap& storageData() const { return m_storageData; }
    bool equipVisible() const { return m_equipVisible; }
    int equipX() const { return m_equipX; }
    int equipY() const { return m_equipY; }
    int equipWidth() const { return m_equipWidth; }
    int equipHeight() const { return m_equipHeight; }
    bool equipMini() const { return m_equipMini; }
    const QVariantMap& equipData() const { return m_equipData; }
    bool skillListVisible() const { return m_skillListVisible; }
    int skillListX() const { return m_skillListX; }
    int skillListY() const { return m_skillListY; }
    int skillListWidth() const { return m_skillListWidth; }
    int skillListHeight() const { return m_skillListHeight; }
    const QVariantMap& skillListData() const { return m_skillListData; }
    bool itemInfoVisible() const { return m_itemInfoVisible; }
    int itemInfoX() const { return m_itemInfoX; }
    int itemInfoY() const { return m_itemInfoY; }
    int itemInfoWidth() const { return m_itemInfoWidth; }
    int itemInfoHeight() const { return m_itemInfoHeight; }
    const QVariantMap& itemInfoData() const { return m_itemInfoData; }
    bool skillDescribeVisible() const { return m_skillDescribeVisible; }
    int skillDescribeX() const { return m_skillDescribeX; }
    int skillDescribeY() const { return m_skillDescribeY; }
    int skillDescribeWidth() const { return m_skillDescribeWidth; }
    int skillDescribeHeight() const { return m_skillDescribeHeight; }
    const QVariantMap& skillDescribeData() const { return m_skillDescribeData; }
    bool itemCollectionVisible() const { return m_itemCollectionVisible; }
    int itemCollectionX() const { return m_itemCollectionX; }
    int itemCollectionY() const { return m_itemCollectionY; }
    int itemCollectionWidth() const { return m_itemCollectionWidth; }
    int itemCollectionHeight() const { return m_itemCollectionHeight; }
    const QVariantMap& itemCollectionData() const { return m_itemCollectionData; }
    bool optionVisible() const { return m_optionVisible; }
    int optionX() const { return m_optionX; }
    int optionY() const { return m_optionY; }
    int optionWidth() const { return m_optionWidth; }
    int optionHeight() const { return m_optionHeight; }
    const QVariantMap& optionData() const { return m_optionData; }
    bool minimapVisible() const { return m_minimapVisible; }
    int minimapX() const { return m_minimapX; }
    int minimapY() const { return m_minimapY; }
    int minimapWidth() const { return m_minimapWidth; }
    int minimapHeight() const { return m_minimapHeight; }
    const QVariantMap& minimapData() const { return m_minimapData; }
    const QVariantList& statusIcons() const { return m_statusIcons; }
    bool shopChoiceVisible() const { return m_shopChoiceVisible; }
    int shopChoiceX() const { return m_shopChoiceX; }
    int shopChoiceY() const { return m_shopChoiceY; }
    int shopChoiceWidth() const { return m_shopChoiceWidth; }
    int shopChoiceHeight() const { return m_shopChoiceHeight; }
    const QString& shopChoiceTitle() const { return m_shopChoiceTitle; }
    const QString& shopChoicePrompt() const { return m_shopChoicePrompt; }
    const QVariantList& shopChoiceButtons() const { return m_shopChoiceButtons; }
    const QVariantList& notifications() const { return m_notifications; }
    const QVariantList& anchors() const { return m_anchors; }

    void setBackendName(const QString& value) {
        if (m_backendName == value) {
            return;
        }
        m_backendName = value;
        emit backendNameChanged();
    }

    void setModeName(const QString& value) {
        if (m_modeName == value) {
            return;
        }
        m_modeName = value;
        emit modeNameChanged();
    }

    void setWallpaperRevision(const QString& value) {
        if (m_wallpaperRevision == value) {
            return;
        }
        m_wallpaperRevision = value;
        emit wallpaperRevisionChanged();
    }

    void setRenderPath(const QString& value) {
        if (m_renderPath == value) {
            return;
        }
        m_renderPath = value;
        emit renderPathChanged();
    }

    void setArchitectureNote(const QString& value) {
        if (m_architectureNote == value) {
            return;
        }
        m_architectureNote = value;
        emit architectureNoteChanged();
    }

    void setLoginStatus(const QString& value) {
        if (m_loginStatus == value) {
            return;
        }
        m_loginStatus = value;
        emit loginStatusChanged();
    }

    void setChatPreview(const QString& value) {
        if (m_chatPreview == value) {
            return;
        }
        m_chatPreview = value;
        emit chatPreviewChanged();
    }

    void setLastInput(const QString& value) {
        if (m_lastInput == value) {
            return;
        }
        m_lastInput = value;
        emit lastInputChanged();
    }

    void setUiScale(double value) {
        if (std::abs(m_uiScale - value) < 0.0001) {
            return;
        }
        m_uiScale = value;
        emit uiScaleChanged();
    }

    void setDebugOverlayData(const QVariantMap& value) {
        if (m_debugOverlayData == value) {
            return;
        }
        m_debugOverlayData = value;
        emit debugOverlayDataChanged();
    }

    void setServerSelectVisible(bool value) {
        if (m_serverSelectVisible == value) {
            return;
        }
        m_serverSelectVisible = value;
        emit serverSelectVisibleChanged();
    }

    void setServerPanelGeometry(int x, int y, int width, int height) {
        if (m_serverPanelX == x && m_serverPanelY == y
            && m_serverPanelWidth == width && m_serverPanelHeight == height) {
            return;
        }
        m_serverPanelX = x;
        m_serverPanelY = y;
        m_serverPanelWidth = width;
        m_serverPanelHeight = height;
        emit serverPanelGeometryChanged();
    }

    void setServerSelectedIndex(int value) {
        if (m_serverSelectedIndex == value) {
            return;
        }
        m_serverSelectedIndex = value;
        emit serverSelectionChanged();
    }

    void setServerHoverIndex(int value) {
        if (m_serverHoverIndex == value) {
            return;
        }
        m_serverHoverIndex = value;
        emit serverHoverIndexChanged();
    }

    void setServerPanelData(const QVariantMap& value) {
        if (m_serverPanelData == value) {
            return;
        }
        m_serverPanelData = value;
        emit serverPanelDataChanged();
    }

    void setServerEntries(const QVariantList& value) {
        m_serverEntries = value;
        emit serverEntriesChanged();
    }

    void setServerEntryText(const QStringList& labels, const QStringList& details) {
        if (m_serverEntryLabels == labels && m_serverEntryDetails == details) {
            return;
        }
        m_serverEntryLabels = labels;
        m_serverEntryDetails = details;
        emit serverEntriesChanged();
    }

    void setLoginPanelVisible(bool value) {
        if (m_loginPanelVisible == value) {
            return;
        }
        m_loginPanelVisible = value;
        emit loginPanelVisibleChanged();
    }

    void setLoginPanelGeometry(int x, int y, int width, int height) {
        if (m_loginPanelX == x && m_loginPanelY == y
            && m_loginPanelWidth == width && m_loginPanelHeight == height) {
            return;
        }
        m_loginPanelX = x;
        m_loginPanelY = y;
        m_loginPanelWidth = width;
        m_loginPanelHeight = height;
        emit loginPanelGeometryChanged();
    }

    void setLoginPanelData(const QString& userId,
        const QString& passwordMask,
        bool saveAccountChecked,
        bool passwordFocused) {
        const bool changed = m_loginUserId != userId
            || m_loginPasswordMask != passwordMask
            || m_loginSaveAccountChecked != saveAccountChecked
            || m_loginPasswordFocused != passwordFocused;
        if (!changed) {
            return;
        }
        m_loginUserId = userId;
        m_loginPasswordMask = passwordMask;
        m_loginSaveAccountChecked = saveAccountChecked;
        m_loginPasswordFocused = passwordFocused;
        emit loginPanelDataChanged();
    }

    void setLoginPanelLabels(const QVariantMap& value) {
        if (m_loginPanelLabels == value) {
            return;
        }
        m_loginPanelLabels = value;
        emit loginPanelLabelsChanged();
    }

    void setLoginButtons(const QVariantList& value) {
        if (m_loginButtons == value) {
            return;
        }
        m_loginButtons = value;
        emit loginButtonsChanged();
    }

    void setCharSelectVisible(bool value) {
        if (m_charSelectVisible == value) {
            return;
        }
        m_charSelectVisible = value;
        emit charSelectVisibleChanged();
    }

    void setCharSelectPanelGeometry(int x, int y, int width, int height) {
        if (m_charSelectPanelX == x && m_charSelectPanelY == y
            && m_charSelectPanelWidth == width && m_charSelectPanelHeight == height) {
            return;
        }
        m_charSelectPanelX = x;
        m_charSelectPanelY = y;
        m_charSelectPanelWidth = width;
        m_charSelectPanelHeight = height;
        emit charSelectPanelGeometryChanged();
    }

    void setCharSelectPageState(int page, int pageCount) {
        if (m_charSelectPage == page && m_charSelectPageCount == pageCount) {
            return;
        }
        m_charSelectPage = page;
        m_charSelectPageCount = pageCount;
        emit charSelectPageChanged();
    }

    void setCharSelectSlots(const QVariantList& value) {
        m_charSelectSlots = value;
        emit charSelectSlotsChanged();
    }

    void setCharSelectSelectedDetails(const QVariantMap& value) {
        m_charSelectSelectedDetails = value;
        emit charSelectSelectedDetailsChanged();
    }

    void setCharSelectPageButtons(const QVariantList& value) {
        if (m_charSelectPageButtons == value) {
            return;
        }
        m_charSelectPageButtons = value;
        emit charSelectPageButtonsChanged();
    }

    void setCharSelectActionButtons(const QVariantList& value) {
        if (m_charSelectActionButtons == value) {
            return;
        }
        m_charSelectActionButtons = value;
        emit charSelectActionButtonsChanged();
    }

    void setMakeCharVisible(bool value) {
        if (m_makeCharVisible == value) {
            return;
        }
        m_makeCharVisible = value;
        emit makeCharVisibleChanged();
    }

    void setMakeCharPanelGeometry(int x, int y, int width, int height) {
        if (m_makeCharPanelX == x && m_makeCharPanelY == y
            && m_makeCharPanelWidth == width && m_makeCharPanelHeight == height) {
            return;
        }
        m_makeCharPanelX = x;
        m_makeCharPanelY = y;
        m_makeCharPanelWidth = width;
        m_makeCharPanelHeight = height;
        emit makeCharPanelGeometryChanged();
    }

    void setMakeCharData(const QString& name,
        bool nameFocused,
        const QVariantList& stats,
        int hairIndex,
        int hairColor) {
        const bool changed = m_makeCharName != name
            || m_makeCharNameFocused != nameFocused
            || m_makeCharStats != stats
            || m_makeCharHairIndex != hairIndex
            || m_makeCharHairColor != hairColor;
        if (!changed) {
            return;
        }
        m_makeCharName = name;
        m_makeCharNameFocused = nameFocused;
        m_makeCharStats = stats;
        m_makeCharHairIndex = hairIndex;
        m_makeCharHairColor = hairColor;
        emit makeCharDataChanged();
    }

    void setMakeCharPanelData(const QVariantMap& value) {
        if (m_makeCharPanelData == value) {
            return;
        }
        m_makeCharPanelData = value;
        emit makeCharPanelDataChanged();
    }

    void setMakeCharButtons(const QVariantList& value) {
        if (m_makeCharButtons == value) {
            return;
        }
        m_makeCharButtons = value;
        emit makeCharButtonsChanged();
    }

    void setMakeCharStatFields(const QVariantList& value) {
        if (m_makeCharStatFields == value) {
            return;
        }
        m_makeCharStatFields = value;
        emit makeCharStatFieldsChanged();
    }

    void setLoadingVisible(bool value) {
        if (m_loadingVisible == value) {
            return;
        }
        m_loadingVisible = value;
        emit loadingVisibleChanged();
    }

    void setLoadingMessage(const QString& value) {
        if (m_loadingMessage == value) {
            return;
        }
        m_loadingMessage = value;
        emit loadingMessageChanged();
    }

    void setLoadingProgress(double value) {
        if (m_loadingProgress == value) {
            return;
        }
        m_loadingProgress = value;
        emit loadingProgressChanged();
    }

    void setNpcMenuVisible(bool value) {
        if (m_npcMenuVisible == value) {
            return;
        }
        m_npcMenuVisible = value;
        emit npcMenuVisibleChanged();
    }

    void setNpcMenuGeometry(int x, int y, int width, int height) {
        if (m_npcMenuX == x && m_npcMenuY == y
            && m_npcMenuWidth == width && m_npcMenuHeight == height) {
            return;
        }
        m_npcMenuX = x;
        m_npcMenuY = y;
        m_npcMenuWidth = width;
        m_npcMenuHeight = height;
        emit npcMenuGeometryChanged();
    }

    void setNpcMenuSelection(int selectedIndex) {
        if (m_npcMenuSelectedIndex == selectedIndex) {
            return;
        }
        m_npcMenuSelectedIndex = selectedIndex;
        emit npcMenuSelectionChanged();
    }

    void setNpcMenuHoverIndex(int hoverIndex) {
        if (m_npcMenuHoverIndex == hoverIndex) {
            return;
        }
        m_npcMenuHoverIndex = hoverIndex;
        emit npcMenuHoverIndexChanged();
    }

    void setNpcMenuButtons(bool okPressed, bool cancelPressed) {
        if (m_npcMenuOkPressed == okPressed && m_npcMenuCancelPressed == cancelPressed) {
            return;
        }
        m_npcMenuOkPressed = okPressed;
        m_npcMenuCancelPressed = cancelPressed;
        emit npcMenuButtonsChanged();
    }

    void setNpcMenuButtonsData(const QVariantList& value) {
        if (m_npcMenuButtons == value) {
            return;
        }
        m_npcMenuButtons = value;
        emit npcMenuButtonsChanged();
    }

    void setNpcMenuOptions(const QVariantList& value) {
        m_npcMenuOptions = value;
        emit npcMenuOptionsChanged();
    }

    void setSayDialogVisible(bool value) {
        if (m_sayDialogVisible == value) {
            return;
        }
        m_sayDialogVisible = value;
        emit sayDialogVisibleChanged();
    }

    void setSayDialogGeometry(int x, int y, int width, int height) {
        if (m_sayDialogX == x && m_sayDialogY == y
            && m_sayDialogWidth == width && m_sayDialogHeight == height) {
            return;
        }
        m_sayDialogX = x;
        m_sayDialogY = y;
        m_sayDialogWidth = width;
        m_sayDialogHeight = height;
        emit sayDialogGeometryChanged();
    }

    void setSayDialogText(const QString& value) {
        if (m_sayDialogText == value) {
            return;
        }
        m_sayDialogText = value;
        emit sayDialogTextChanged();
    }

    void setSayDialogAction(bool hasAction,
        const QString& label,
        bool hovered,
        bool pressed) {
        const bool changed = m_sayDialogHasAction != hasAction
            || m_sayDialogActionLabel != label
            || m_sayDialogActionHovered != hovered
            || m_sayDialogActionPressed != pressed;
        if (!changed) {
            return;
        }
        m_sayDialogHasAction = hasAction;
        m_sayDialogActionLabel = label;
        m_sayDialogActionHovered = hovered;
        m_sayDialogActionPressed = pressed;
        emit sayDialogActionChanged();
    }

    void setSayDialogActionButton(const QVariantMap& value) {
        if (m_sayDialogActionButton == value) {
            return;
        }
        m_sayDialogActionButton = value;
        emit sayDialogActionChanged();
    }

    void setNpcInputVisible(bool value) {
        if (m_npcInputVisible == value) {
            return;
        }
        m_npcInputVisible = value;
        emit npcInputVisibleChanged();
    }

    void setNpcInputGeometry(int x, int y, int width, int height) {
        if (m_npcInputX == x && m_npcInputY == y
            && m_npcInputWidth == width && m_npcInputHeight == height) {
            return;
        }
        m_npcInputX = x;
        m_npcInputY = y;
        m_npcInputWidth = width;
        m_npcInputHeight = height;
        emit npcInputGeometryChanged();
    }

    void setNpcInputText(const QString& label, const QString& text) {
        if (m_npcInputLabel == label && m_npcInputText == text) {
            return;
        }
        m_npcInputLabel = label;
        m_npcInputText = text;
        emit npcInputTextChanged();
    }

    void setNpcInputButtons(bool okPressed, bool cancelPressed) {
        if (m_npcInputOkPressed == okPressed && m_npcInputCancelPressed == cancelPressed) {
            return;
        }
        m_npcInputOkPressed = okPressed;
        m_npcInputCancelPressed = cancelPressed;
        emit npcInputButtonsChanged();
    }

    void setNpcInputButtonsData(const QVariantList& value) {
        if (m_npcInputButtons == value) {
            return;
        }
        m_npcInputButtons = value;
        emit npcInputButtonsChanged();
    }

    void setChooseMenuVisible(bool value) {
        if (m_chooseMenuVisible == value) {
            return;
        }
        m_chooseMenuVisible = value;
        emit chooseMenuVisibleChanged();
    }

    void setChooseMenuGeometry(int x, int y, int width, int height) {
        if (m_chooseMenuX == x && m_chooseMenuY == y
            && m_chooseMenuWidth == width && m_chooseMenuHeight == height) {
            return;
        }
        m_chooseMenuX = x;
        m_chooseMenuY = y;
        m_chooseMenuWidth = width;
        m_chooseMenuHeight = height;
        emit chooseMenuGeometryChanged();
    }

    void setChooseMenuSelectedIndex(int value) {
        if (m_chooseMenuSelectedIndex == value) {
            return;
        }
        m_chooseMenuSelectedIndex = value;
        emit chooseMenuSelectedIndexChanged();
    }

    void setChooseMenuPressedIndex(int value) {
        if (m_chooseMenuPressedIndex == value) {
            return;
        }
        m_chooseMenuPressedIndex = value;
        emit chooseMenuPressedIndexChanged();
    }

    void setChooseMenuOptions(const QVariantList& value) {
        if (m_chooseMenuOptions == value) {
            return;
        }
        m_chooseMenuOptions = value;
        emit chooseMenuOptionsChanged();
    }

    void setItemShopVisible(bool value) {
        if (m_itemShopVisible == value) {
            return;
        }
        m_itemShopVisible = value;
        emit itemShopVisibleChanged();
    }

    void setItemShopGeometry(int x, int y, int width, int height) {
        if (m_itemShopX == x && m_itemShopY == y
            && m_itemShopWidth == width && m_itemShopHeight == height) {
            return;
        }
        m_itemShopX = x;
        m_itemShopY = y;
        m_itemShopWidth = width;
        m_itemShopHeight = height;
        emit itemShopGeometryChanged();
    }

    void setItemShopTitle(const QString& value) {
        if (m_itemShopTitle == value) {
            return;
        }
        m_itemShopTitle = value;
        emit itemShopTitleChanged();
    }

    void setItemShopData(const QVariantMap& value) {
        if (m_itemShopData == value) {
            return;
        }
        m_itemShopData = value;
        emit itemShopDataChanged();
    }

    void setItemShopRows(const QVariantList& value) {
        m_itemShopRows = value;
        emit itemShopRowsChanged();
    }

    void setItemPurchaseVisible(bool value) {
        if (m_itemPurchaseVisible == value) {
            return;
        }
        m_itemPurchaseVisible = value;
        emit itemPurchaseVisibleChanged();
    }

    void setItemPurchaseGeometry(int x, int y, int width, int height) {
        if (m_itemPurchaseX == x && m_itemPurchaseY == y
            && m_itemPurchaseWidth == width && m_itemPurchaseHeight == height) {
            return;
        }
        m_itemPurchaseX = x;
        m_itemPurchaseY = y;
        m_itemPurchaseWidth = width;
        m_itemPurchaseHeight = height;
        emit itemPurchaseGeometryChanged();
    }

    void setItemPurchaseTotal(int value) {
        if (m_itemPurchaseTotal == value) {
            return;
        }
        m_itemPurchaseTotal = value;
        emit itemPurchaseTotalChanged();
    }

    void setItemPurchaseData(const QVariantMap& value) {
        if (m_itemPurchaseData == value) {
            return;
        }
        m_itemPurchaseData = value;
        emit itemPurchaseDataChanged();
    }

    void setItemPurchaseRows(const QVariantList& value) {
        m_itemPurchaseRows = value;
        emit itemPurchaseRowsChanged();
    }

    void setItemPurchaseButtons(const QVariantList& value) {
        m_itemPurchaseButtons = value;
        emit itemPurchaseButtonsChanged();
    }

    void setItemSellVisible(bool value) {
        if (m_itemSellVisible == value) {
            return;
        }
        m_itemSellVisible = value;
        emit itemSellVisibleChanged();
    }

    void setItemSellGeometry(int x, int y, int width, int height) {
        if (m_itemSellX == x && m_itemSellY == y
            && m_itemSellWidth == width && m_itemSellHeight == height) {
            return;
        }
        m_itemSellX = x;
        m_itemSellY = y;
        m_itemSellWidth = width;
        m_itemSellHeight = height;
        emit itemSellGeometryChanged();
    }

    void setItemSellTotal(int value) {
        if (m_itemSellTotal == value) {
            return;
        }
        m_itemSellTotal = value;
        emit itemSellTotalChanged();
    }

    void setItemSellData(const QVariantMap& value) {
        if (m_itemSellData == value) {
            return;
        }
        m_itemSellData = value;
        emit itemSellDataChanged();
    }

    void setItemSellRows(const QVariantList& value) {
        m_itemSellRows = value;
        emit itemSellRowsChanged();
    }

    void setItemSellButtons(const QVariantList& value) {
        m_itemSellButtons = value;
        emit itemSellButtonsChanged();
    }

    void setShortCutVisible(bool value) {
        if (m_shortCutVisible == value) {
            return;
        }
        m_shortCutVisible = value;
        emit shortCutVisibleChanged();
    }

    void setShortCutGeometry(int x, int y, int width, int height) {
        if (m_shortCutX == x && m_shortCutY == y
            && m_shortCutWidth == width && m_shortCutHeight == height) {
            return;
        }
        m_shortCutX = x;
        m_shortCutY = y;
        m_shortCutWidth = width;
        m_shortCutHeight = height;
        emit shortCutGeometryChanged();
    }

    void setShortCutPage(int value) {
        if (m_shortCutPage == value) {
            return;
        }
        m_shortCutPage = value;
        emit shortCutPageChanged();
    }

    void setShortCutHoverSlot(int value) {
        if (m_shortCutHoverSlot == value) {
            return;
        }
        m_shortCutHoverSlot = value;
        emit shortCutHoverSlotChanged();
    }

    void setShortCutSlots(const QVariantList& value) {
        if (m_shortCutSlots == value) {
            return;
        }
        m_shortCutSlots = value;
        emit shortCutSlotsChanged();
    }

    void setBasicInfoVisible(bool value) {
        if (m_basicInfoVisible == value) {
            return;
        }
        m_basicInfoVisible = value;
        emit basicInfoVisibleChanged();
    }

    void setBasicInfoGeometry(int x, int y, int width, int height) {
        if (m_basicInfoX == x && m_basicInfoY == y
            && m_basicInfoWidth == width && m_basicInfoHeight == height) {
            return;
        }
        m_basicInfoX = x;
        m_basicInfoY = y;
        m_basicInfoWidth = width;
        m_basicInfoHeight = height;
        emit basicInfoGeometryChanged();
    }

    void setBasicInfoMini(bool value) {
        if (m_basicInfoMini == value) {
            return;
        }
        m_basicInfoMini = value;
        emit basicInfoMiniChanged();
    }

    void setBasicInfoData(const QVariantMap& value) {
        m_basicInfoData = value;
        emit basicInfoDataChanged();
    }

    void setStatusVisible(bool value) {
        if (m_statusVisible == value) {
            return;
        }
        m_statusVisible = value;
        emit statusVisibleChanged();
    }

    void setStatusGeometry(int x, int y, int width, int height) {
        if (m_statusX == x && m_statusY == y
            && m_statusWidth == width && m_statusHeight == height) {
            return;
        }
        m_statusX = x;
        m_statusY = y;
        m_statusWidth = width;
        m_statusHeight = height;
        emit statusGeometryChanged();
    }

    void setStatusMini(bool value) {
        if (m_statusMini == value) {
            return;
        }
        m_statusMini = value;
        emit statusMiniChanged();
    }

    void setStatusPage(int value) {
        if (m_statusPage == value) {
            return;
        }
        m_statusPage = value;
        emit statusPageChanged();
    }

    void setStatusData(const QVariantMap& value) {
        m_statusData = value;
        emit statusDataChanged();
    }

    void setChatWindowVisible(bool value) {
        if (m_chatWindowVisible == value) {
            return;
        }
        m_chatWindowVisible = value;
        emit chatWindowVisibleChanged();
    }

    void setChatWindowGeometry(int x, int y, int width, int height) {
        if (m_chatWindowX == x && m_chatWindowY == y
            && m_chatWindowWidth == width && m_chatWindowHeight == height) {
            return;
        }
        m_chatWindowX = x;
        m_chatWindowY = y;
        m_chatWindowWidth = width;
        m_chatWindowHeight = height;
        emit chatWindowGeometryChanged();
    }

    void setChatWindowInputActive(bool value) {
        if (m_chatWindowInputActive == value) {
            return;
        }
        m_chatWindowInputActive = value;
        emit chatWindowInputActiveChanged();
    }

    void setChatWindowWhisperInputActive(bool value) {
        if (m_chatWindowWhisperInputActive == value) {
            return;
        }
        m_chatWindowWhisperInputActive = value;
        emit chatWindowWhisperInputActiveChanged();
    }

    void setChatWindowMessageInputActive(bool value) {
        if (m_chatWindowMessageInputActive == value) {
            return;
        }
        m_chatWindowMessageInputActive = value;
        emit chatWindowMessageInputActiveChanged();
    }

    void setChatWindowWhisperTargetText(const QString& value) {
        if (m_chatWindowWhisperTargetText == value) {
            return;
        }
        m_chatWindowWhisperTargetText = value;
        emit chatWindowWhisperTargetTextChanged();
    }

    void setChatWindowInputText(const QString& value) {
        if (m_chatWindowInputText == value) {
            return;
        }
        m_chatWindowInputText = value;
        emit chatWindowInputTextChanged();
    }

    void setChatWindowLines(const QVariantList& value) {
        m_chatWindowLines = value;
        emit chatWindowLinesChanged();
    }

    void setChatWindowScrollBar(const QVariantMap& value) {
        m_chatWindowScrollBar = value;
        emit chatWindowScrollBarChanged();
    }

    void setChatWindowUi(const QVariantMap& value) {
        m_chatWindowUi = value;
        emit chatWindowUiChanged();
    }

    void setRechargeGaugeVisible(bool value) {
        if (m_rechargeGaugeVisible == value) {
            return;
        }
        m_rechargeGaugeVisible = value;
        emit rechargeGaugeVisibleChanged();
    }

    void setRechargeGaugeGeometry(int x, int y, int width, int height) {
        if (m_rechargeGaugeX == x && m_rechargeGaugeY == y
            && m_rechargeGaugeWidth == width && m_rechargeGaugeHeight == height) {
            return;
        }
        m_rechargeGaugeX = x;
        m_rechargeGaugeY = y;
        m_rechargeGaugeWidth = width;
        m_rechargeGaugeHeight = height;
        emit rechargeGaugeGeometryChanged();
    }

    void setRechargeGaugeProgress(int amount, int total) {
        if (m_rechargeGaugeAmount == amount && m_rechargeGaugeTotal == total) {
            return;
        }
        m_rechargeGaugeAmount = amount;
        m_rechargeGaugeTotal = total;
        emit rechargeGaugeProgressChanged();
    }

    void setInventoryVisible(bool value) {
        if (m_inventoryVisible == value) {
            return;
        }
        m_inventoryVisible = value;
        emit inventoryVisibleChanged();
    }

    void setInventoryGeometry(int x, int y, int width, int height) {
        if (m_inventoryX == x && m_inventoryY == y
            && m_inventoryWidth == width && m_inventoryHeight == height) {
            return;
        }
        m_inventoryX = x;
        m_inventoryY = y;
        m_inventoryWidth = width;
        m_inventoryHeight = height;
        emit inventoryGeometryChanged();
    }

    void setInventoryMini(bool value) {
        if (m_inventoryMini == value) {
            return;
        }
        m_inventoryMini = value;
        emit inventoryMiniChanged();
    }

    void setInventoryTab(int value) {
        if (m_inventoryTab == value) {
            return;
        }
        m_inventoryTab = value;
        emit inventoryTabChanged();
    }

    void setInventoryData(const QVariantMap& value) {
        if (m_inventoryData == value) {
            return;
        }
        m_inventoryData = value;
        emit inventoryDataChanged();
    }

    void setStorageVisible(bool value) {
        if (m_storageVisible == value) {
            return;
        }
        m_storageVisible = value;
        emit storageVisibleChanged();
    }

    void setStorageGeometry(int x, int y, int width, int height) {
        if (m_storageX == x && m_storageY == y
            && m_storageWidth == width && m_storageHeight == height) {
            return;
        }
        m_storageX = x;
        m_storageY = y;
        m_storageWidth = width;
        m_storageHeight = height;
        emit storageGeometryChanged();
    }

    void setStorageMini(bool value) {
        if (m_storageMini == value) {
            return;
        }
        m_storageMini = value;
        emit storageMiniChanged();
    }

    void setStorageTab(int value) {
        if (m_storageTab == value) {
            return;
        }
        m_storageTab = value;
        emit storageTabChanged();
    }

    void setStorageData(const QVariantMap& value) {
        if (m_storageData == value) {
            return;
        }
        m_storageData = value;
        emit storageDataChanged();
    }

    void setEquipVisible(bool value) {
        if (m_equipVisible == value) {
            return;
        }
        m_equipVisible = value;
        emit equipVisibleChanged();
    }

    void setEquipGeometry(int x, int y, int width, int height) {
        if (m_equipX == x && m_equipY == y
            && m_equipWidth == width && m_equipHeight == height) {
            return;
        }
        m_equipX = x;
        m_equipY = y;
        m_equipWidth = width;
        m_equipHeight = height;
        emit equipGeometryChanged();
    }

    void setEquipMini(bool value) {
        if (m_equipMini == value) {
            return;
        }
        m_equipMini = value;
        emit equipMiniChanged();
    }

    void setEquipData(const QVariantMap& value) {
        if (m_equipData == value) {
            return;
        }
        m_equipData = value;
        emit equipDataChanged();
    }

    void setSkillListVisible(bool value) {
        if (m_skillListVisible == value) {
            return;
        }
        m_skillListVisible = value;
        emit skillListVisibleChanged();
    }

    void setSkillListGeometry(int x, int y, int width, int height) {
        if (m_skillListX == x && m_skillListY == y
            && m_skillListWidth == width && m_skillListHeight == height) {
            return;
        }
        m_skillListX = x;
        m_skillListY = y;
        m_skillListWidth = width;
        m_skillListHeight = height;
        emit skillListGeometryChanged();
    }

    void setSkillListData(const QVariantMap& value) {
        if (m_skillListData == value) {
            return;
        }
        m_skillListData = value;
        emit skillListDataChanged();
    }

    void setItemInfoVisible(bool value) {
        if (m_itemInfoVisible == value) {
            return;
        }
        m_itemInfoVisible = value;
        emit itemInfoVisibleChanged();
    }

    void setItemInfoGeometry(int x, int y, int width, int height) {
        if (m_itemInfoX == x && m_itemInfoY == y
            && m_itemInfoWidth == width && m_itemInfoHeight == height) {
            return;
        }
        m_itemInfoX = x;
        m_itemInfoY = y;
        m_itemInfoWidth = width;
        m_itemInfoHeight = height;
        emit itemInfoGeometryChanged();
    }

    void setItemInfoData(const QVariantMap& value) {
        if (m_itemInfoData == value) {
            return;
        }
        m_itemInfoData = value;
        emit itemInfoDataChanged();
    }

    void setSkillDescribeVisible(bool value) {
        if (m_skillDescribeVisible == value) {
            return;
        }
        m_skillDescribeVisible = value;
        emit skillDescribeVisibleChanged();
    }

    void setSkillDescribeGeometry(int x, int y, int width, int height) {
        if (m_skillDescribeX == x && m_skillDescribeY == y
            && m_skillDescribeWidth == width && m_skillDescribeHeight == height) {
            return;
        }
        m_skillDescribeX = x;
        m_skillDescribeY = y;
        m_skillDescribeWidth = width;
        m_skillDescribeHeight = height;
        emit skillDescribeGeometryChanged();
    }

    void setSkillDescribeData(const QVariantMap& value) {
        if (m_skillDescribeData == value) {
            return;
        }
        m_skillDescribeData = value;
        emit skillDescribeDataChanged();
    }

    void setItemCollectionVisible(bool value) {
        if (m_itemCollectionVisible == value) {
            return;
        }
        m_itemCollectionVisible = value;
        emit itemCollectionVisibleChanged();
    }

    void setItemCollectionGeometry(int x, int y, int width, int height) {
        if (m_itemCollectionX == x && m_itemCollectionY == y
            && m_itemCollectionWidth == width && m_itemCollectionHeight == height) {
            return;
        }
        m_itemCollectionX = x;
        m_itemCollectionY = y;
        m_itemCollectionWidth = width;
        m_itemCollectionHeight = height;
        emit itemCollectionGeometryChanged();
    }

    void setItemCollectionData(const QVariantMap& value) {
        if (m_itemCollectionData == value) {
            return;
        }
        m_itemCollectionData = value;
        emit itemCollectionDataChanged();
    }

    void setOptionVisible(bool value) {
        if (m_optionVisible == value) {
            return;
        }
        m_optionVisible = value;
        emit optionVisibleChanged();
    }

    void setOptionGeometry(int x, int y, int width, int height) {
        if (m_optionX == x && m_optionY == y
            && m_optionWidth == width && m_optionHeight == height) {
            return;
        }
        m_optionX = x;
        m_optionY = y;
        m_optionWidth = width;
        m_optionHeight = height;
        emit optionGeometryChanged();
    }

    void setOptionData(const QVariantMap& value) {
        m_optionData = value;
        emit optionDataChanged();
    }

    void setMinimapVisible(bool value) {
        if (m_minimapVisible == value) {
            return;
        }
        m_minimapVisible = value;
        emit minimapVisibleChanged();
    }

    void setMinimapGeometry(int x, int y, int width, int height) {
        if (m_minimapX == x && m_minimapY == y
            && m_minimapWidth == width && m_minimapHeight == height) {
            return;
        }
        m_minimapX = x;
        m_minimapY = y;
        m_minimapWidth = width;
        m_minimapHeight = height;
        emit minimapGeometryChanged();
    }

    void setMinimapData(const QVariantMap& value) {
        m_minimapData = value;
        emit minimapDataChanged();
    }

    void setStatusIcons(const QVariantList& value) {
        m_statusIcons = value;
        emit statusIconsChanged();
    }

    void setShopChoiceVisible(bool value) {
        if (m_shopChoiceVisible == value) {
            return;
        }
        m_shopChoiceVisible = value;
        emit shopChoiceVisibleChanged();
    }

    void setShopChoiceGeometry(int x, int y, int width, int height) {
        if (m_shopChoiceX == x && m_shopChoiceY == y
            && m_shopChoiceWidth == width && m_shopChoiceHeight == height) {
            return;
        }
        m_shopChoiceX = x;
        m_shopChoiceY = y;
        m_shopChoiceWidth = width;
        m_shopChoiceHeight = height;
        emit shopChoiceGeometryChanged();
    }

    void setShopChoiceText(const QString& title, const QString& prompt) {
        if (m_shopChoiceTitle == title && m_shopChoicePrompt == prompt) {
            return;
        }
        m_shopChoiceTitle = title;
        m_shopChoicePrompt = prompt;
        emit shopChoiceTextChanged();
    }

    void setShopChoiceButtons(const QVariantList& value) {
        m_shopChoiceButtons = value;
        emit shopChoiceButtonsChanged();
    }

    void setNotifications(const QVariantList& value) {
        m_notifications = value;
        emit notificationsChanged();
    }

    void setAnchors(const QVariantList& value) {
        m_anchors = value;
        emit anchorsChanged();
    }

signals:
    void backendNameChanged();
    void modeNameChanged();
    void wallpaperRevisionChanged();
    void renderPathChanged();
    void architectureNoteChanged();
    void loginStatusChanged();
    void chatPreviewChanged();
    void lastInputChanged();
    void uiScaleChanged();
    void debugOverlayDataChanged();
    void serverSelectVisibleChanged();
    void serverPanelGeometryChanged();
    void serverSelectionChanged();
    void serverHoverIndexChanged();
    void serverPanelDataChanged();
    void serverEntriesChanged();
    void loginPanelVisibleChanged();
    void loginPanelGeometryChanged();
    void loginPanelDataChanged();
    void loginPanelLabelsChanged();
    void loginButtonsChanged();
    void charSelectVisibleChanged();
    void charSelectPanelGeometryChanged();
    void charSelectPageChanged();
    void charSelectSlotsChanged();
    void charSelectSelectedDetailsChanged();
    void charSelectPageButtonsChanged();
    void charSelectActionButtonsChanged();
    void makeCharVisibleChanged();
    void makeCharPanelGeometryChanged();
    void makeCharDataChanged();
    void makeCharPanelDataChanged();
    void makeCharButtonsChanged();
    void makeCharStatFieldsChanged();
    void loadingVisibleChanged();
    void loadingMessageChanged();
    void loadingProgressChanged();
    void npcMenuVisibleChanged();
    void npcMenuGeometryChanged();
    void npcMenuSelectionChanged();
    void npcMenuHoverIndexChanged();
    void npcMenuButtonsChanged();
    void npcMenuOptionsChanged();
    void sayDialogVisibleChanged();
    void sayDialogGeometryChanged();
    void sayDialogTextChanged();
    void sayDialogActionChanged();
    void npcInputVisibleChanged();
    void npcInputGeometryChanged();
    void npcInputTextChanged();
    void npcInputButtonsChanged();
    void chooseMenuVisibleChanged();
    void chooseMenuGeometryChanged();
    void chooseMenuSelectedIndexChanged();
    void chooseMenuPressedIndexChanged();
    void chooseMenuOptionsChanged();
    void itemShopVisibleChanged();
    void itemShopGeometryChanged();
    void itemShopTitleChanged();
    void itemShopDataChanged();
    void itemShopRowsChanged();
    void itemPurchaseVisibleChanged();
    void itemPurchaseGeometryChanged();
    void itemPurchaseTotalChanged();
    void itemPurchaseDataChanged();
    void itemPurchaseRowsChanged();
    void itemPurchaseButtonsChanged();
    void itemSellVisibleChanged();
    void itemSellGeometryChanged();
    void itemSellTotalChanged();
    void itemSellDataChanged();
    void itemSellRowsChanged();
    void itemSellButtonsChanged();
    void shortCutVisibleChanged();
    void shortCutGeometryChanged();
    void shortCutPageChanged();
    void shortCutHoverSlotChanged();
    void shortCutSlotsChanged();
    void basicInfoVisibleChanged();
    void basicInfoGeometryChanged();
    void basicInfoMiniChanged();
    void basicInfoDataChanged();
    void statusVisibleChanged();
    void statusGeometryChanged();
    void statusMiniChanged();
    void statusPageChanged();
    void statusDataChanged();
    void chatWindowVisibleChanged();
    void chatWindowGeometryChanged();
    void chatWindowInputActiveChanged();
    void chatWindowWhisperInputActiveChanged();
    void chatWindowMessageInputActiveChanged();
    void chatWindowWhisperTargetTextChanged();
    void chatWindowInputTextChanged();
    void chatWindowLinesChanged();
    void chatWindowScrollBarChanged();
    void chatWindowUiChanged();
    void rechargeGaugeVisibleChanged();
    void rechargeGaugeGeometryChanged();
    void rechargeGaugeProgressChanged();
    void inventoryVisibleChanged();
    void inventoryGeometryChanged();
    void inventoryMiniChanged();
    void inventoryTabChanged();
    void inventoryDataChanged();
    void storageVisibleChanged();
    void storageGeometryChanged();
    void storageMiniChanged();
    void storageTabChanged();
    void storageDataChanged();
    void equipVisibleChanged();
    void equipGeometryChanged();
    void equipMiniChanged();
    void equipDataChanged();
    void skillListVisibleChanged();
    void skillListGeometryChanged();
    void skillListDataChanged();
    void itemInfoVisibleChanged();
    void itemInfoGeometryChanged();
    void itemInfoDataChanged();
    void skillDescribeVisibleChanged();
    void skillDescribeGeometryChanged();
    void skillDescribeDataChanged();
    void itemCollectionVisibleChanged();
    void itemCollectionGeometryChanged();
    void itemCollectionDataChanged();
    void optionVisibleChanged();
    void optionGeometryChanged();
    void optionDataChanged();
    void minimapVisibleChanged();
    void minimapGeometryChanged();
    void minimapDataChanged();
    void statusIconsChanged();
    void shopChoiceVisibleChanged();
    void shopChoiceGeometryChanged();
    void shopChoiceTextChanged();
    void shopChoiceButtonsChanged();
    void notificationsChanged();
    void anchorsChanged();

private:
    QString m_backendName;
    QString m_modeName;
    QString m_wallpaperRevision;
    QString m_renderPath;
    QString m_architectureNote;
    QString m_loginStatus;
    QString m_chatPreview;
    QString m_lastInput;
    double m_uiScale = 1.0;
    QVariantMap m_debugOverlayData;
    bool m_serverSelectVisible = false;
    int m_serverPanelX = 0;
    int m_serverPanelY = 0;
    int m_serverPanelWidth = 0;
    int m_serverPanelHeight = 0;
    int m_serverSelectedIndex = -1;
    int m_serverHoverIndex = -1;
    QVariantMap m_serverPanelData;
    QVariantList m_serverEntries;
    QStringList m_serverEntryLabels;
    QStringList m_serverEntryDetails;
    bool m_loginPanelVisible = false;
    int m_loginPanelX = 0;
    int m_loginPanelY = 0;
    int m_loginPanelWidth = 0;
    int m_loginPanelHeight = 0;
    QString m_loginUserId;
    QString m_loginPasswordMask;
    bool m_loginSaveAccountChecked = false;
    bool m_loginPasswordFocused = false;
    QVariantMap m_loginPanelLabels;
    QVariantList m_loginButtons;
    bool m_charSelectVisible = false;
    int m_charSelectPanelX = 0;
    int m_charSelectPanelY = 0;
    int m_charSelectPanelWidth = 0;
    int m_charSelectPanelHeight = 0;
    int m_charSelectPage = 0;
    int m_charSelectPageCount = 0;
    QVariantList m_charSelectSlots;
    QVariantMap m_charSelectSelectedDetails;
    QVariantList m_charSelectPageButtons;
    QVariantList m_charSelectActionButtons;
    bool m_makeCharVisible = false;
    int m_makeCharPanelX = 0;
    int m_makeCharPanelY = 0;
    int m_makeCharPanelWidth = 0;
    int m_makeCharPanelHeight = 0;
    QString m_makeCharName;
    bool m_makeCharNameFocused = false;
    QVariantList m_makeCharStats;
    int m_makeCharHairIndex = 0;
    int m_makeCharHairColor = 0;
    QVariantMap m_makeCharPanelData;
    QVariantList m_makeCharButtons;
    QVariantList m_makeCharStatFields;
    bool m_loadingVisible = false;
    QString m_loadingMessage;
    double m_loadingProgress = 0.0;
    bool m_npcMenuVisible = false;
    int m_npcMenuX = 0;
    int m_npcMenuY = 0;
    int m_npcMenuWidth = 0;
    int m_npcMenuHeight = 0;
    int m_npcMenuSelectedIndex = -1;
    int m_npcMenuHoverIndex = -1;
    bool m_npcMenuOkPressed = false;
    bool m_npcMenuCancelPressed = false;
    QVariantList m_npcMenuOptions;
    QVariantList m_npcMenuButtons;
    bool m_sayDialogVisible = false;
    int m_sayDialogX = 0;
    int m_sayDialogY = 0;
    int m_sayDialogWidth = 0;
    int m_sayDialogHeight = 0;
    QString m_sayDialogText;
    bool m_sayDialogHasAction = false;
    QString m_sayDialogActionLabel;
    bool m_sayDialogActionHovered = false;
    bool m_sayDialogActionPressed = false;
    QVariantMap m_sayDialogActionButton;
    bool m_npcInputVisible = false;
    int m_npcInputX = 0;
    int m_npcInputY = 0;
    int m_npcInputWidth = 0;
    int m_npcInputHeight = 0;
    QString m_npcInputLabel;
    QString m_npcInputText;
    bool m_npcInputOkPressed = false;
    bool m_npcInputCancelPressed = false;
    QVariantList m_npcInputButtons;
    bool m_chooseMenuVisible = false;
    int m_chooseMenuX = 0;
    int m_chooseMenuY = 0;
    int m_chooseMenuWidth = 0;
    int m_chooseMenuHeight = 0;
    int m_chooseMenuSelectedIndex = -1;
    int m_chooseMenuPressedIndex = -1;
    QVariantList m_chooseMenuOptions;
    bool m_itemShopVisible = false;
    int m_itemShopX = 0;
    int m_itemShopY = 0;
    int m_itemShopWidth = 0;
    int m_itemShopHeight = 0;
    QString m_itemShopTitle;
    QVariantMap m_itemShopData;
    QVariantList m_itemShopRows;
    bool m_itemPurchaseVisible = false;
    int m_itemPurchaseX = 0;
    int m_itemPurchaseY = 0;
    int m_itemPurchaseWidth = 0;
    int m_itemPurchaseHeight = 0;
    int m_itemPurchaseTotal = 0;
    QVariantMap m_itemPurchaseData;
    QVariantList m_itemPurchaseRows;
    QVariantList m_itemPurchaseButtons;
    bool m_itemSellVisible = false;
    int m_itemSellX = 0;
    int m_itemSellY = 0;
    int m_itemSellWidth = 0;
    int m_itemSellHeight = 0;
    int m_itemSellTotal = 0;
    QVariantMap m_itemSellData;
    QVariantList m_itemSellRows;
    QVariantList m_itemSellButtons;
    bool m_shortCutVisible = false;
    int m_shortCutX = 0;
    int m_shortCutY = 0;
    int m_shortCutWidth = 0;
    int m_shortCutHeight = 0;
    int m_shortCutPage = 0;
    int m_shortCutHoverSlot = -1;
    QVariantList m_shortCutSlots;
    bool m_basicInfoVisible = false;
    int m_basicInfoX = 0;
    int m_basicInfoY = 0;
    int m_basicInfoWidth = 0;
    int m_basicInfoHeight = 0;
    bool m_basicInfoMini = false;
    QVariantMap m_basicInfoData;
    bool m_statusVisible = false;
    int m_statusX = 0;
    int m_statusY = 0;
    int m_statusWidth = 0;
    int m_statusHeight = 0;
    bool m_statusMini = false;
    int m_statusPage = 0;
    QVariantMap m_statusData;
    bool m_chatWindowVisible = false;
    int m_chatWindowX = 0;
    int m_chatWindowY = 0;
    int m_chatWindowWidth = 0;
    int m_chatWindowHeight = 0;
    bool m_chatWindowInputActive = false;
    bool m_chatWindowWhisperInputActive = false;
    bool m_chatWindowMessageInputActive = false;
    QString m_chatWindowWhisperTargetText;
    QString m_chatWindowInputText;
    QVariantList m_chatWindowLines;
    QVariantMap m_chatWindowScrollBar;
    QVariantMap m_chatWindowUi;
    bool m_rechargeGaugeVisible = false;
    int m_rechargeGaugeX = 0;
    int m_rechargeGaugeY = 0;
    int m_rechargeGaugeWidth = 0;
    int m_rechargeGaugeHeight = 0;
    int m_rechargeGaugeAmount = 0;
    int m_rechargeGaugeTotal = 0;
    bool m_inventoryVisible = false;
    int m_inventoryX = 0;
    int m_inventoryY = 0;
    int m_inventoryWidth = 0;
    int m_inventoryHeight = 0;
    bool m_inventoryMini = false;
    int m_inventoryTab = 0;
    QVariantMap m_inventoryData;
    bool m_storageVisible = false;
    int m_storageX = 0;
    int m_storageY = 0;
    int m_storageWidth = 0;
    int m_storageHeight = 0;
    bool m_storageMini = false;
    int m_storageTab = 0;
    QVariantMap m_storageData;
    bool m_equipVisible = false;
    int m_equipX = 0;
    int m_equipY = 0;
    int m_equipWidth = 0;
    int m_equipHeight = 0;
    bool m_equipMini = false;
    QVariantMap m_equipData;
    bool m_skillListVisible = false;
    int m_skillListX = 0;
    int m_skillListY = 0;
    int m_skillListWidth = 0;
    int m_skillListHeight = 0;
    QVariantMap m_skillListData;
    bool m_itemInfoVisible = false;
    int m_itemInfoX = 0;
    int m_itemInfoY = 0;
    int m_itemInfoWidth = 0;
    int m_itemInfoHeight = 0;
    QVariantMap m_itemInfoData;
    bool m_skillDescribeVisible = false;
    int m_skillDescribeX = 0;
    int m_skillDescribeY = 0;
    int m_skillDescribeWidth = 0;
    int m_skillDescribeHeight = 0;
    QVariantMap m_skillDescribeData;
    bool m_itemCollectionVisible = false;
    int m_itemCollectionX = 0;
    int m_itemCollectionY = 0;
    int m_itemCollectionWidth = 0;
    int m_itemCollectionHeight = 0;
    QVariantMap m_itemCollectionData;
    bool m_optionVisible = false;
    int m_optionX = 0;
    int m_optionY = 0;
    int m_optionWidth = 0;
    int m_optionHeight = 0;
    QVariantMap m_optionData;
    bool m_minimapVisible = false;
    int m_minimapX = 0;
    int m_minimapY = 0;
    int m_minimapWidth = 0;
    int m_minimapHeight = 0;
    QVariantMap m_minimapData;
    QVariantList m_statusIcons;
    bool m_shopChoiceVisible = false;
    int m_shopChoiceX = 0;
    int m_shopChoiceY = 0;
    int m_shopChoiceWidth = 0;
    int m_shopChoiceHeight = 0;
    QString m_shopChoiceTitle;
    QString m_shopChoicePrompt;
    QVariantList m_shopChoiceButtons;
    QVariantList m_notifications;
    QVariantList m_anchors;
};
