#ifndef WINDOWSPLATFORMADAPTER_H
#define WINDOWSPLATFORMADAPTER_H

#include "PlatformAdapter.h"
#include <QHash>
#include <QWidget>
#include <QEventLoop>
#include <QTimer>
#include <windows.h>
#include <psapi.h>

class WindowsPlatformAdapter : public PlatformAdapter
{
public:
    WindowsPlatformAdapter();
    ~WindowsPlatformAdapter() override;

    void simulateClick(int x, int y, MouseButton button, ClickAction action, int holdDuration = 100) override;
    void simulateClickNoInterference(int x, int y, MouseButton button, ClickAction action, ClickMethod method = ClickMethod::NoInterference) override;
    void simulateClickToWindow(uintptr_t windowId, int relX, int relY, MouseButton button, ClickAction action) override;
    void simulateMouseMove(int x, int y) override;
    QPoint getMousePosition() override;

    bool registerHotkey(int id, int key, int modifiers, std::function<void()> callback) override;
    bool unregisterHotkey(int id) override;
    bool handleHotkeyEvent(int id) override;

    WindowInfo getActiveWindow() override;
    std::vector<WindowInfo> listWindows() override;
    bool setForegroundWindow(uintptr_t windowId) override;
    uintptr_t getForegroundWindowId() override;
    std::wstring getWindowTitleW(uintptr_t windowId) override;
    std::string getWindowTitle(uintptr_t windowId) override;
    QPoint getWindowPosition(uintptr_t windowId) override;
    WindowInfo getWindowInfo(uintptr_t windowId) override;

    // Minimized window handling
    bool isWindowMinimized(uintptr_t windowId) override;
    WindowInfo getWindowPlacementInfo(uintptr_t windowId) override;

    uintptr_t getWindowAtPoint(int x, int y) override;
    void highlightWindow(uintptr_t windowId, int durationMs = 500) override;

    bool startMouseHook(std::function<void(const RecordedClick&)> callback) override;
    void stopMouseHook() override;

    ElementInfo findElementByText(uintptr_t windowId, const std::wstring& text) override;
    void clickElement(uintptr_t elementId, MouseButton button, ClickAction action) override;

private:
    void sendMouseEvent(DWORD flags, DWORD data = 0);
    void nonBlockingWait(int ms);  // v2.2: non-blocking wait that processes Qt events
    DWORD mapMouseButton(MouseButton button);
    int mapKeyToVirtualKey(int key);
    int mapModifiersToModFlags(int modifiers);
    bool shouldFilterClick(int x, int y);  // Check if click is inside AutoClicker window

    QHash<int, std::function<void()>> m_hotkeyCallbacks;
    HWND m_hwnd;
    HHOOK m_mouseHook = nullptr;
    std::function<void(const RecordedClick&)> m_mouseHookCallback;

    static WindowsPlatformAdapter* s_instance;  // For hook callback
    static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
};

#endif // WINDOWSPLATFORMADAPTER_H