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

enum class ClickMode {
    FixedPosition,
    FollowCursor,
    Sequence,
    RandomArea,
    Drag
};

enum class ClickMethod {
    NoInterference,  // No interference - direct message to window (PostMessage)
    SimulateMouse,   // Simulate physical mouse (SendInput)
    Auto             // Auto select based on situation
};

enum class RecordedButton {
    Left,
    Right,
    Middle
};

struct RecordedClick {
    int x, y;
    RecordedButton button;
};

struct WindowInfo {
    uintptr_t id;
    std::string title;
    std::string processName;
    int x, y, width, height;
};

struct ElementInfo {
    uintptr_t windowId;
    std::string text;
    std::string className;
    int relativeX, relativeY;  // Position relative to parent window
    int width, height;
};

class PlatformAdapter
{
public:
    virtual ~PlatformAdapter() = default;

    // Mouse operations
    virtual void simulateClick(int x, int y, MouseButton button, ClickAction action, int holdDuration = 100) = 0;
    virtual void simulateClickNoInterference(int x, int y, MouseButton button, ClickAction action, ClickMethod method = ClickMethod::NoInterference) = 0;
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
    virtual uintptr_t getForegroundWindowId() = 0;
    virtual std::string getWindowTitle(uintptr_t windowId) = 0;
    virtual QPoint getWindowPosition(uintptr_t windowId) = 0;

    // Recording operations
    virtual bool startMouseHook(std::function<void(const RecordedClick&)> callback) = 0;
    virtual void stopMouseHook() = 0;

    // Element finding operations
    virtual ElementInfo findElementByText(uintptr_t windowId, const std::string& text) = 0;
    virtual void clickElement(uintptr_t elementId, MouseButton button, ClickAction action) = 0;
};

#endif // PLATFORMADAPTER_H