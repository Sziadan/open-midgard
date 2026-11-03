#pragma once

#include <QString>

class CGameMode;
class QObject;
class QtUiState;
enum class RenderBackendType;

class QtUiStateAdapter {
public:
    QtUiStateAdapter();
    ~QtUiStateAdapter();

    QObject* stateObject() const;
    void setLastInput(const QString& value);
    void syncChat();
    bool syncMenu(RenderBackendType activeBackend,
        RenderBackendType nativeOverlayBackend);
    bool syncGameplay(CGameMode& mode,
        RenderBackendType activeBackend,
        RenderBackendType nativeOverlayBackend,
        int mouseX,
        int mouseY);

private:
    QtUiState* m_state;
};
