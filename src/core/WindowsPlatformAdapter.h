#ifndef WINDOWSPLATFORMADAPTER_H
#define WINDOWSPLATFORMADAPTER_H

#include "PlatformAdapter.h"
#include <QHash>
#include <QWidget>
#include <windows.h>
#include <psapi.h>

class WindowsPlatformAdapter : public PlatformAdapter
{
public:
    WindowsPlatformAdapter();
    ~WindowsPlatformAdapter() override;

    void simulateClick(int x, int y, MouseButton button, ClickAction action, int holdDuration = 100) override;
    void simulateClickNoInterference(int x, int y, MouseButton button, ClickAction action, ClickMethod method = ClickMethod::NoInterference) override;
    void simulateMouseMove(int x, int y) override;
    QPoint getMousePosition() override;

    bool registerHotkey(int id, int key, int modifiers, std::function<void()> callback) override;
    bool unregisterHotkey(int id) override;
    bool handleHotkeyEvent(int id) override;

    WindowInfo getActiveWindow() override;
    std::vector<WindowInfo> listWindows() override;
    bool setForegroundWindow(uintptr_t windowId) override;

private:
    void sendMouseEvent(DWORD flags, DWORD data = 0);
    DWORD mapMouseButton(MouseButton button);
    int mapKeyToVirtualKey(int key);
    int mapModifiersToModFlags(int modifiers);

    QHash<int, std::function<void()>> m_hotkeyCallbacks;
    HWND m_hwnd;
};

#endif // WINDOWSPLATFORMADAPTER_H