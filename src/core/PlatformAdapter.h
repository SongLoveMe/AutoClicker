#ifndef PLATFORMADAPTER_H
#define PLATFORMADAPTER_H

#include <functional>
#include <string>
#include <vector>
#include <QPoint>
#include <QString>

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

// Convert RecordedButton to MouseButton
inline MouseButton toMouseButton(RecordedButton rb) {
    switch (rb) {
        case RecordedButton::Left: return MouseButton::Left;
        case RecordedButton::Right: return MouseButton::Right;
        case RecordedButton::Middle: return MouseButton::Middle;
    }
    return MouseButton::Left;  // Default
}

// Enhanced sequence point with window binding
struct SequencePoint {
    int screenX, screenY;           // Absolute screen coordinates at recording time
    RecordedButton button;          // Mouse button used

    // Window binding (0 means no binding / absolute screen coordinates)
    uintptr_t windowId;
    int relX, relY;                 // Coordinates relative to window top-left

    // Window identification (Unicode)
    std::wstring windowTitle;       // Window title for display and matching
    std::wstring processName;       // Process name for additional matching

    // User customization
    QString userLabel;              // Optional custom label (e.g., "Button OK")
};

struct RecordedClick {
    int x, y;
    RecordedButton button;
    uintptr_t windowId;             // Window under cursor when clicked
};

struct WindowInfo {
    uintptr_t id = 0;
    std::wstring title;             // Unicode window title
    std::wstring processName;       // Unicode process name
    int x = 0, y = 0, width = 0, height = 0;
};

struct ElementInfo {
    uintptr_t windowId;
    std::wstring text;              // Unicode element text
    std::wstring className;         // Unicode class name
    int relativeX, relativeY;       // Position relative to parent window
    int width, height;
};

class PlatformAdapter
{
public:
    virtual ~PlatformAdapter() = default;

    // Mouse operations
    virtual void simulateClick(int x, int y, MouseButton button, ClickAction action, int holdDuration = 100) = 0;
    virtual void simulateClickNoInterference(int x, int y, MouseButton button, ClickAction action, ClickMethod method = ClickMethod::NoInterference) = 0;
    virtual void simulateClickToWindow(uintptr_t windowId, int relX, int relY, MouseButton button, ClickAction action) = 0;  // Direct window message
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

    // Unicode title (preferred)
    virtual std::wstring getWindowTitleW(uintptr_t windowId) = 0;
    // Legacy ANSI (for compatibility)
    virtual std::string getWindowTitle(uintptr_t windowId) = 0;

    virtual QPoint getWindowPosition(uintptr_t windowId) = 0;
    virtual WindowInfo getWindowInfo(uintptr_t windowId) = 0;

    // Minimized window handling - uses GetWindowPlacement for correct position
    virtual bool isWindowMinimized(uintptr_t windowId) = 0;
    virtual WindowInfo getWindowPlacementInfo(uintptr_t windowId) = 0;

    virtual uintptr_t getWindowAtPoint(int x, int y) = 0;

    // Window highlight for visual feedback
    virtual void highlightWindow(uintptr_t windowId, int durationMs = 500) = 0;

    // Recording operations
    virtual bool startMouseHook(std::function<void(const RecordedClick&)> callback) = 0;
    virtual void stopMouseHook() = 0;

    // Element finding operations
    virtual ElementInfo findElementByText(uintptr_t windowId, const std::wstring& text) = 0;
    virtual void clickElement(uintptr_t elementId, MouseButton button, ClickAction action) = 0;
};

#endif // PLATFORMADAPTER_H