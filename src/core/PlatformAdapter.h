#ifndef PLATFORMADAPTER_H
#define PLATFORMADAPTER_H

#include <functional>
#include <string>
#include <vector>
#include <QPoint>

enum class MouseButton {
    Left,
    Right,
    Middle
};

enum class ClickAction {
    Single,
    Double,
    Triple,
    Hold
};

struct WindowInfo {
    uintptr_t id;
    std::string title;
    std::string processName;
    int x, y, width, height;
};

class PlatformAdapter
{
public:
    virtual ~PlatformAdapter() = default;

    // Mouse operations
    virtual void simulateClick(int x, int y, MouseButton button, ClickAction action, int holdDuration = 100) = 0;
    virtual void simulateMouseMove(int x, int y) = 0;
    virtual QPoint getMousePosition() = 0;

    // Hotkey operations
    virtual bool registerHotkey(int id, int key, int modifiers, std::function<void()> callback) = 0;
    virtual bool unregisterHotkey(int id) = 0;
    virtual bool handleHotkeyEvent(int id) = 0;

    // Window operations
    virtual WindowInfo getActiveWindow() = 0;
    virtual std::vector<WindowInfo> listWindows() = 0;
    virtual bool setForegroundWindow(uintptr_t windowId) = 0;
};

#endif // PLATFORMADAPTER_H