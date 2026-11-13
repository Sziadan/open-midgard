#include "ui/UISkillSelectWnd.h"
#include "ui/UIItemCompositionWnd.h"

void CheckCraftingChoices(QGuiApplication& app, QQuickView& view, QtUiState* state,
    CGameMode& mode, QtUiStateAdapter& adapter, const QString& output)
{
    const auto sync = [&] {
        Require(adapter.syncGameplay(mode, RenderBackendType::LegacyDirect3D7,
            RenderBackendType::LegacyDirect3D7, 0, 0), "crafting choice projection failed");
        Frames(app, view);
    };
    const auto capture = [&](const QString& name, unsigned expectedItem) {
        auto* window = g_windowMgr.m_skillSelectWnd;
        UISkillSelectWnd::DisplayData display{};
        Require(window && window->IsCraftingSelection() && window->GetDisplayDataForQt(&display)
            && (expectedItem ? display.rows.size() == 1 && display.rows.front().itemId == expectedItem
                : display.productionConfirmation && display.rows.empty() && display.materialSlots.size() == 3),
            "crafting choice lost the offered item");
        sync();
        auto* panel = Find(view.rootObject(), "skillChoiceWindow");
        auto* row = panel ? Find(panel, "skillChoiceRow0") : nullptr;
        auto* label = row ? Find(row, "skillChoiceLabel") : nullptr;
        Require(panel && panel->isVisible() && (!expectedItem || (row && row->isVisible() && label && label->isVisible()
            && label->property("text").toString() == QString::fromStdString(display.rows.front().label))),
            "Qt crafting choice lost the native item label");
        ArgbDibSurface dib;
        Require(dib.EnsureSize(view.width(), view.height()), "crafting choice DIB failed");
        std::fill_n(dib.GetPixels(), view.width() * view.height(), 0xff181b20u);
        window->DrawToHdc(dib.GetDC());
        const auto native = QImage(reinterpret_cast<uchar*>(dib.GetPixels()), view.width(), view.height(),
            view.width() * 4, QImage::Format_RGB32).copy();
        const auto qt = view.grabWindow().convertToFormat(QImage::Format_RGB32);
        QRect textArea(window->m_x + 8, window->m_y + 27, window->m_w - 16, 30);
        if (expectedItem) {
            const auto& first = display.rows.front();
            const int left = first.x + (first.showIcon ? 34 : 8);
            textArea = QRect(left, first.y + 4, first.width - (left - first.x) - 40, 12);
        }
        Require(native.rect().contains(textArea), "native crafting choice label fell outside capture");
        int ink = 0;
        int lightPixels = 0;
        for (int y = textArea.top(); y <= textArea.bottom(); ++y) {
            for (int x = textArea.left(); x <= textArea.right(); ++x) {
                const int value = native.pixelColor(x, y).value();
                if (value < 100) ++ink;
                if (value > 180) ++lightPixels;
            }
        }
        Require(ink > 4 && lightPixels > 20 && !qt.isNull() && state->skillSelectVisible(),
            "crafting choice did not render text and row background");
        for (size_t i = 0; i < display.materialSlots.size(); ++i) {
            const auto& slot = display.materialSlots[i];
            auto* frame = Find(panel, "skillChoiceMaterial" + QString::number(i));
            Require(frame && frame->isVisible() && frame->width() == 32 && frame->height() == 32
                && frame->x() == slot.x - window->m_x && frame->y() == slot.y - window->m_y
                && native.pixelColor(slot.x + 2, slot.y + 2).value() > 180,
                "production material slot was missing from native or Qt rendering");
        }
        Require(native.save(output + "/" + name + ".native.png")
            && qt.save(output + "/" + name + ".qml.png"), "crafting choice capture failed");
    };

    Dispatch(mode, {0x8d, 1, 12, 0, 0xb1, 4, 0, 0, 0, 0, 0, 0});
    capture("craft-production", 1201);
    auto* production = g_windowMgr.m_skillSelectWnd;
    production->HandleKeyDown(VK_DOWN);
    production->HandleKeyDown(VK_RETURN);
    capture("craft-production-confirm", 0);
    auto* recipe = Find(view.rootObject(), "skillChoiceRecipe");
    Require(recipe && recipe->isVisible() && recipe->property("text").toString() == "1 Iron\n10 Jellopy"
        && recipe->property("contentHeight").toReal() <= recipe->height(),
        "production confirmation did not display the full recipe description");
    UISkillSelectWnd::DisplayData display;
    Require(production->GetDisplayDataForQt(&display), "production confirmation did not project its slots");
    ITEM_INFO material{};
    material.SetItemId(1000);
    material.m_itemIndex = 99;
    material.m_num = 1;
    material.m_itemType = 3;
    g_session.SetInventoryItem(material);
    auto* inventory = g_windowMgr.m_itemWnd;
    const bool createdInventory = inventory == nullptr;
    if (createdInventory) {
        inventory = static_cast<UIItemWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_ITEMWND));
    }
    Require(inventory != nullptr, "production render fixture could not open inventory");
    const int oldTab = inventory->GetCurrentTab();
    const QRect oldBounds(inventory->m_x, inventory->m_y, inventory->m_w, inventory->m_h);
    inventory->SetShow(1);
    inventory->SetCurrentTab(2);
    inventory->ResizeInventory(280, 120);
    inventory->Move(production->m_x + production->m_w + 10, production->m_y);
    sync();
    auto* tile = Find(view.rootObject(), "commerceTile_inventory_99");
    Require(tile && tile->isVisible(), (QString("production inventory material unavailable (tile %1, tab %2, rows %3, mini %4)")
        .arg(tile != nullptr).arg(inventory->GetCurrentTab())
        .arg(state->commerceData().value("inventoryItems").toList().size()).arg(inventory->IsMiniMode())).toUtf8().constData());
    const auto& slot = display.materialSlots.front();
    const QPointF start = tile->mapToScene(QPointF(tile->width() / 2, tile->height() / 2));
    const QPointF finish(slot.x + 8, slot.y + 8);
    QMouseEvent press(QEvent::MouseButtonPress, start, view.mapToGlobal(start.toPoint()),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent move(QEvent::MouseMove, finish, view.mapToGlobal(finish.toPoint()),
        Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, finish, view.mapToGlobal(finish.toPoint()),
        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&view, &press);
    QCoreApplication::sendEvent(&view, &move);
    QCoreApplication::sendEvent(&view, &release);
    sync();
    Require(state->skillSelectData().value("materialSlots").toList().front().toMap().value("itemId").toUInt() == 1000,
        "Qt inventory drop did not fill the production slot");
    capture("craft-production-material", 0);
    if (createdInventory) {
        g_windowMgr.DeleteWindow(inventory);
    } else {
        inventory->SetShow(0);
        inventory->SetCurrentTab(oldTab);
        inventory->Resize(oldBounds.width(), oldBounds.height());
        inventory->Move(oldBounds.x(), oldBounds.y());
    }
    g_session.RemoveInventoryItem(99, 1);
    Dispatch(mode, {0x21, 2, 17, 0, 2, 0, 0xb1, 4, 3, 0, 0, 0, 0, 0, 0, 0, 0});
    capture("craft-weapon-refine", 1201);
    Require(!recipe->isVisible(), "recipe text leaked into the weapon refine chooser");
    Dispatch(mode, {0xfc, 1, 17, 0, 0, 0, 0xb1, 4, 3, 0, 0, 0, 0, 0, 0, 0, 0});
    capture("craft-repair", 1201);
    Dispatch(mode, {0x5a, 2, 8, 0, 1, 0, 0xad, 0x2e});
    capture("craft-cooking", 11949);
    mode.ClearCraftingSelection();
    sync();
    auto* panel = Find(view.rootObject(), "skillChoiceWindow");
    Require(!g_windowMgr.m_skillSelectWnd && panel && !panel->isVisible(),
        "crafting choice teardown retained a projected window");

    ITEM_INFO card{};
    card.m_itemIndex=100;
    card.SetItemId(4001);
    card.m_itemType=6;
    card.m_num=1;
    g_session.SetInventoryItem(card);
    ITEM_INFO equipment{};
    equipment.m_itemIndex=101;
    equipment.SetItemId(1201);
    equipment.m_itemType=5;
    equipment.m_isIdentified=1;
    equipment.m_num=1;
    g_session.SetInventoryItem(equipment);
    mode.m_lastCardItemIndex=100;
    Dispatch(mode,{0x7b,1,6,0,101,0});
    auto* composition=g_windowMgr.m_itemCompositionWnd;
    UIItemCompositionWnd::DisplayData choice;
    Require(composition && composition->GetDisplayDataForQt(&choice) && choice.rows.size()==1,
        "card composition list did not show its offered equipment");
    sync();
    auto* compositionPanel=Find(view.rootObject(),"itemCompositionWindow");
    auto* row=compositionPanel?Find(compositionPanel,"itemCompositionRow101"):nullptr;
    auto* label=row?Find(row,"itemCompositionItemLabel"):nullptr;
    Require(label && label->isVisible() && label->property("text").toString()==QString::fromStdString(choice.rows.front().label),
        "Qt composition chooser did not show the equipment label");
    ArgbDibSurface dib;
    Require(dib.EnsureSize(view.width(),view.height()),"composition native surface failed");
    std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);
    composition->DrawToHdc(dib.GetDC());
    const QImage native=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy();
    const auto qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);
    const auto& shown=choice.rows.front();
    const QRect nativeLabel(shown.x+34,shown.y+4,shown.width-72,12);
    const auto qtLabel=label->mapRectToScene(label->boundingRect()).toAlignedRect().intersected(qt.rect());
    Require(native.rect().contains(nativeLabel) && PixelsNear(native,nativeLabel,0xff000000u)>4
        && PixelsNear(qt,qtLabel,0xff25313cu)>4,"composition equipment label did not paint");
    Require(native.save(output+"/card-composition.native.png") && qt.save(output+"/card-composition.qml.png"),
        "composition chooser capture failed");
    const auto& cancel=choice.cancelButton;
    g_windowMgr.OnLBtnDown(cancel.x+4,cancel.y+4);
    g_windowMgr.OnLBtnUp(cancel.x+4,cancel.y+4);
    sync();
    Require(!g_windowMgr.m_itemCompositionWnd && !compositionPanel->isVisible() && !mode.m_lastCardItemIndex,
        "card composition cancel left the chooser or card selection active");
    g_session.RemoveInventoryItem(100,1);
    g_session.RemoveInventoryItem(101,1);
}
