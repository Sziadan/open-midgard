#include "ui/UISkillSelectWnd.h"

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
            && display.rows.size() == 1 && display.rows.front().itemId == expectedItem,
            "crafting choice lost the offered item");
        sync();
        auto* panel = Find(view.rootObject(), "skillChoiceWindow");
        auto* row = panel ? Find(panel, "skillChoiceRow0") : nullptr;
        auto* label = row ? Find(row, "skillChoiceLabel") : nullptr;
        Require(panel && panel->isVisible() && row && row->isVisible() && label && label->isVisible()
            && label->property("text").toString() == QString::fromStdString(display.rows.front().label),
            "Qt crafting choice lost the native item label");
        ArgbDibSurface dib;
        Require(dib.EnsureSize(view.width(), view.height()), "crafting choice DIB failed");
        std::fill_n(dib.GetPixels(), view.width() * view.height(), 0xff181b20u);
        window->DrawToHdc(dib.GetDC());
        const auto native = QImage(reinterpret_cast<uchar*>(dib.GetPixels()), view.width(), view.height(),
            view.width() * 4, QImage::Format_RGB32).copy();
        const auto qt = view.grabWindow().convertToFormat(QImage::Format_RGB32);
        const auto& first = display.rows.front();
        const int left = first.x + (first.showIcon ? 34 : 8);
        const QRect textArea(left, first.y + 4, first.width - (left - first.x) - 40, 12);
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
}
