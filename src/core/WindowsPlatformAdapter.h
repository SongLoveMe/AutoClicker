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
    uintptr_t getForegroundWindowId() override;
    std::string getWindowTitle(uintptr_t windowId) override;
    QPoint getWindowPosition(uintptr_t windowId) override;

    bool startMouseHook(std::function<void(const RecordedClick&)> callback) override;
    void stopMouseHook() override;

    ElementInfo findElementByText(uintptr_t windowId, const std::string& text) override;
    void clickElement(uintptr_t elementId, MouseButton button, ClickAction action) override;

private:
    void sendMouseEvent(DWORD flags, DWORD data = 0);
    DWORD mapMouseButton(MouseButton button);
    int mapKeyToVirtualKey(int key);
    int mapModifiersToModFlags(int modifiers);

    QHash<int, std::function<void()>> m_hotkeyCallbacks;
    HWND m_hwnd;
    HHOOK m_mouseHook = nullptr;
    std::function<void(const RecordedClick&)> m_mouseHookCallback;

    static WindowsPlatformAdapter* s_instance;  // For hook callback
    static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
};

#endif // WINDOWSPLATFORMADAPTER_H