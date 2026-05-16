#include "WindowsPlatformAdapter.h"
#include <QApplication>
#include <QGuiApplication>
#include <QDebug>
#include <QThread>

WindowsPlatformAdapter::WindowsPlatformAdapter()
    : m_hwnd(nullptr)
{
}

WindowsPlatformAdapter::~WindowsPlatformAdapter()
{
    // Stop mouse hook if active
    stopMouseHook();

    // Unregister all hotkeys
    for (auto it = m_hotkeyCallbacks.begin(); it != m_hotkeyCallbacks.end(); ++it) {
        UnregisterHotKey(m_hwnd, it.key());
    }
}

void WindowsPlatformAdapter::simulateClick(int x, int y, MouseButton button, ClickAction action, int holdDuration)
{
    // Move to position first
    simulateMouseMove(x, y);

    DWORD downFlag = 0;
    DWORD upFlag = 0;

    switch (button) {
        case MouseButton::Left:
            downFlag = MOUSEEVENTF_LEFTDOWN;
            upFlag = MOUSEEVENTF_LEFTUP;
            break;
        case MouseButton::Right:
            downFlag = MOUSEEVENTF_RIGHTDOWN;
            upFlag = MOUSEEVENTF_RIGHTUP;
            break;
        case MouseButton::Middle:
            downFlag = MOUSEEVENTF_MIDDLEDOWN;
            upFlag = MOUSEEVENTF_MIDDLEUP;
            break;
    }

    int clickCount = 1;
    switch (action) {
        case ClickAction::Single:
            clickCount = 1;
            break;
        case ClickAction::Double:
            clickCount = 2;
            break;
        case ClickAction::Triple:
            clickCount = 3;
            break;
        case ClickAction::Hold:
            clickCount = 1;
            break;
    }

    for (int i = 0; i < clickCount; ++i) {
        // Mouse down
        INPUT inputDown = {};
        inputDown.type = INPUT_MOUSE;
        inputDown.mi.dwFlags = downFlag;
        SendInput(1, &inputDown, sizeof(INPUT));

        if (action == ClickAction::Hold) {
            Sleep(holdDuration);
        } else {
            Sleep(10);
        }

        // Mouse up
        INPUT inputUp = {};
        inputUp.type = INPUT_MOUSE;
        inputUp.mi.dwFlags = upFlag;
        SendInput(1, &inputUp, sizeof(INPUT));

        if (i < clickCount - 1) {
            Sleep(50);
        }
    }
}

void WindowsPlatformAdapter::simulateClickNoInterference(int x, int y, MouseButton button, ClickAction action, ClickMethod method)
{
    if (method == ClickMethod::SimulateMouse) {
        // Use traditional SendInput method
        simulateClick(x, y, button, action);
        return;
    }

    // Get top-level window at point
    POINT pt = {x, y};
    HWND childWindow = WindowFromPoint(pt);
    if (!childWindow) {
        simulateClick(x, y, button, action);
        return;
    }

    // Get top-level parent window (critical fix for message delivery)
    HWND topLevelWindow = GetAncestor(childWindow, GA_ROOT);
    if (!topLevelWindow) {
        topLevelWindow = childWindow;
    }

    // Convert to client area coordinates
    POINT clientPt = pt;
    ScreenToClient(topLevelWindow, &clientPt);

    // Determine button flags
    WPARAM wParam = 0;
    switch (button) {
        case MouseButton::Left:
            wParam = MK_LBUTTON;
            break;
        case MouseButton::Right:
            wParam = MK_RBUTTON;
            break;
        case MouseButton::Middle:
            wParam = MK_MBUTTON;
            break;
    }

    LPARAM lParam = MAKELPARAM(clientPt.x, clientPt.y);

    int clickCount = 1;
    switch (action) {
        case ClickAction::Single:
            clickCount = 1;
            break;
        case ClickAction::Double:
            clickCount = 2;
            break;
        case ClickAction::Triple:
            clickCount = 3;
            break;
        case ClickAction::Hold:
            // For hold, use traditional method
            simulateClick(x, y, button, action);
            return;
    }

    // Determine message types
    UINT downMsg, upMsg;
    switch (button) {
        case MouseButton::Left:
            downMsg = WM_LBUTTONDOWN;
            upMsg = WM_LBUTTONUP;
            break;
        case MouseButton::Right:
            downMsg = WM_RBUTTONDOWN;
            upMsg = WM_RBUTTONUP;
            break;
        case MouseButton::Middle:
            downMsg = WM_MBUTTONDOWN;
            upMsg = WM_MBUTTONUP;
            break;
    }

    // Use SendMessageTimeout for synchronous delivery (more reliable than PostMessage)
    DWORD_PTR result;

    for (int i = 0; i < clickCount; ++i) {
        SendMessageTimeoutW(topLevelWindow, downMsg, wParam, lParam,
                           SMTO_BLOCK | SMTO_ABORTIFHUNG, 100, &result);
        SendMessageTimeoutW(topLevelWindow, upMsg, 0, lParam,
                           SMTO_BLOCK | SMTO_ABORTIFHUNG, 100, &result);

        if (i < clickCount - 1) {
            QThread::msleep(50);  // Use Qt's thread sleep
        }
    }

    // For double-click, also send WM_LBUTTONDBLCLK
    if (action == ClickAction::Double && button == MouseButton::Left) {
        SendMessageTimeoutW(topLevelWindow, WM_LBUTTONDBLCLK, wParam, lParam,
                           SMTO_BLOCK | SMTO_ABORTIFHUNG, 100, &result);
    }
}

void WindowsPlatformAdapter::simulateClickToWindow(uintptr_t windowId, int relX, int relY, MouseButton button, ClickAction action)
{
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    if (!hwnd) return;

    // Determine button flags and messages
    WPARAM wParam = 0;
    UINT downMsg, upMsg;

    switch (button) {
        case MouseButton::Left:
            wParam = MK_LBUTTON;
            downMsg = WM_LBUTTONDOWN;
            upMsg = WM_LBUTTONUP;
            break;
        case MouseButton::Right:
            wParam = MK_RBUTTON;
            downMsg = WM_RBUTTONDOWN;
            upMsg = WM_RBUTTONUP;
            break;
        case MouseButton::Middle:
            wParam = MK_MBUTTON;
            downMsg = WM_MBUTTONDOWN;
            upMsg = WM_MBUTTONUP;
            break;
    }

    LPARAM lParam = MAKELPARAM(relX, relY);

    int clickCount = 1;
    switch (action) {
        case ClickAction::Single: clickCount = 1; break;
        case ClickAction::Double: clickCount = 2; break;
        case ClickAction::Triple: clickCount = 3; break;
        case ClickAction::Hold: clickCount = 1; break;
    }

    // Use SendMessageTimeout for synchronous delivery
    DWORD_PTR result;

    for (int i = 0; i < clickCount; ++i) {
        SendMessageTimeoutW(hwnd, downMsg, wParam, lParam,
                           SMTO_BLOCK | SMTO_ABORTIFHUNG, 100, &result);
        SendMessageTimeoutW(hwnd, upMsg, 0, lParam,
                           SMTO_BLOCK | SMTO_ABORTIFHUNG, 100, &result);

        if (action == ClickAction::Hold) {
            QThread::msleep(100);
        }

        if (i < clickCount - 1) {
            QThread::msleep(50);
        }
    }

    // For double-click, also send WM_LBUTTONDBLCLK
    if (action == ClickAction::Double && button == MouseButton::Left) {
        SendMessageTimeoutW(hwnd, WM_LBUTTONDBLCLK, wParam, lParam,
                           SMTO_BLOCK | SMTO_ABORTIFHUNG, 100, &result);
    }
}

void WindowsPlatformAdapter::simulateMouseMove(int x, int y)
{
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dx = static_cast<LONG>(x * 65535.0 / GetSystemMetrics(SM_CXSCREEN));
    input.mi.dy = static_cast<LONG>(y * 65535.0 / GetSystemMetrics(SM_CYSCREEN));
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    SendInput(1, &input, sizeof(INPUT));
}

QPoint WindowsPlatformAdapter::getMousePosition()
{
    POINT pt;
    GetCursorPos(&pt);
    return QPoint(pt.x, pt.y);
}

bool WindowsPlatformAdapter::registerHotkey(int id, int key, int modifiers, std::function<void()> callback)
{
    // Get window handle
    if (!m_hwnd) {
        QWidget* mainWindow = QApplication::topLevelWidgets().isEmpty() ? nullptr : QApplication::topLevelWidgets().first();
        if (mainWindow) {
            m_hwnd = reinterpret_cast<HWND>(mainWindow->winId());
        }
    }

    if (!m_hwnd) {
        qWarning() << "Cannot register hotkey: no window handle";
        return false;
    }

    int vk = mapKeyToVirtualKey(key);
    int mod = mapModifiersToModFlags(modifiers);

    if (RegisterHotKey(m_hwnd, id, mod, vk)) {
        m_hotkeyCallbacks[id] = callback;
        qDebug() << "Hotkey registered: id=" << id << "key=" << key;
        return true;
    }

    qWarning() << "Failed to register hotkey: id=" << id << "error=" << GetLastError();
    return false;
}

bool WindowsPlatformAdapter::unregisterHotkey(int id)
{
    if (UnregisterHotKey(m_hwnd, id)) {
        m_hotkeyCallbacks.remove(id);
        return true;
    }
    return false;
}

bool WindowsPlatformAdapter::handleHotkeyEvent(int id)
{
    if (m_hotkeyCallbacks.contains(id)) {
        m_hotkeyCallbacks[id]();
        return true;
    }
    return false;
}

WindowInfo WindowsPlatformAdapter::getActiveWindow()
{
    HWND hwnd = GetForegroundWindow();
    return getWindowInfo(reinterpret_cast<uintptr_t>(hwnd));
}

std::vector<WindowInfo> WindowsPlatformAdapter::listWindows()
{
    std::vector<WindowInfo> windows;

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* list = reinterpret_cast<std::vector<WindowInfo>*>(lParam);

        if (IsWindowVisible(hwnd)) {
            int length = GetWindowTextLengthW(hwnd);
            if (length > 0) {
                WindowInfo info;
                info.id = reinterpret_cast<uintptr_t>(hwnd);

                // Use Unicode API
                std::wstring title;
                title.resize(length + 1);
                GetWindowTextW(hwnd, &title[0], length + 1);
                title.resize(length);  // Remove null terminator
                info.title = title;

                RECT rect;
                GetWindowRect(hwnd, &rect);
                info.x = rect.left;
                info.y = rect.top;
                info.width = rect.right - rect.left;
                info.height = rect.bottom - rect.top;

                // Get process name
                DWORD processId;
                GetWindowThreadProcessId(hwnd, &processId);
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
                if (hProcess) {
                    WCHAR processName[MAX_PATH];
                    GetModuleFileNameExW(hProcess, nullptr, processName, MAX_PATH);
                    info.processName = processName;
                    CloseHandle(hProcess);
                }

                list->push_back(info);
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&windows));

    return windows;
}

bool WindowsPlatformAdapter::setForegroundWindow(uintptr_t windowId)
{
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    return SetForegroundWindow(hwnd) != 0;
}

int WindowsPlatformAdapter::mapKeyToVirtualKey(int key)
{
    if (key >= 'A' && key <= 'Z') return key;
    if (key >= '0' && key <= '9') return key;

    switch (key) {
        case Qt::Key_F1: return VK_F1;
        case Qt::Key_F2: return VK_F2;
        case Qt::Key_F3: return VK_F3;
        case Qt::Key_F4: return VK_F4;
        case Qt::Key_F5: return VK_F5;
        case Qt::Key_F6: return VK_F6;
        case Qt::Key_F7: return VK_F7;
        case Qt::Key_F8: return VK_F8;
        case Qt::Key_F9: return VK_F9;
        case Qt::Key_F10: return VK_F10;
        case Qt::Key_F11: return VK_F11;
        case Qt::Key_F12: return VK_F12;
        case Qt::Key_Escape: return VK_ESCAPE;
        case Qt::Key_Return: return VK_RETURN;
        case Qt::Key_Enter: return VK_RETURN;
        case Qt::Key_Tab: return VK_TAB;
        case Qt::Key_Space: return VK_SPACE;
        case Qt::Key_Control: return VK_CONTROL;
        case Qt::Key_Shift: return VK_SHIFT;
        case Qt::Key_Alt: return VK_MENU;
        default: return key;
    }
}

int WindowsPlatformAdapter::mapModifiersToModFlags(int modifiers)
{
    int mod = 0;

    if (modifiers & Qt::ControlModifier) mod |= MOD_CONTROL;
    if (modifiers & Qt::ShiftModifier) mod |= MOD_SHIFT;
    if (modifiers & Qt::AltModifier) mod |= MOD_ALT;
    if (modifiers & Qt::MetaModifier) mod |= MOD_WIN;

    return mod;
}

// Static member initialization
WindowsPlatformAdapter* WindowsPlatformAdapter::s_instance = nullptr;

bool WindowsPlatformAdapter::shouldFilterClick(int x, int y)
{
    if (!m_hwnd) return false;

    HWND autoClickerWindow = GetAncestor(m_hwnd, GA_ROOT);
    if (!autoClickerWindow) return false;

    // Get the window at the click position
    POINT pt = {x, y};
    HWND clickedWindow = WindowFromPoint(pt);

    // Check if the clicked window belongs to AutoClicker (either root or child)
    HWND clickedRoot = GetAncestor(clickedWindow, GA_ROOT);

    return (clickedRoot == autoClickerWindow);
}

// Mouse hook implementation
LRESULT CALLBACK WindowsPlatformAdapter::MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && s_instance && s_instance->m_mouseHookCallback) {
        MSLLHOOKSTRUCT* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);

        // Filter clicks inside the AutoClicker window
        if (s_instance->shouldFilterClick(info->pt.x, info->pt.y)) {
            return CallNextHookEx(s_instance->m_mouseHook, nCode, wParam, lParam);
        }

        RecordedButton button = RecordedButton::Left;
        bool isClick = false;

        if (wParam == WM_LBUTTONDOWN) {
            button = RecordedButton::Left;
            isClick = true;
        } else if (wParam == WM_RBUTTONDOWN) {
            button = RecordedButton::Right;
            isClick = true;
        } else if (wParam == WM_MBUTTONDOWN) {
            button = RecordedButton::Middle;
            isClick = true;
        }

        if (isClick) {
            RecordedClick click;
            click.x = info->pt.x;
            click.y = info->pt.y;
            click.button = button;

            // Capture window at click position
            HWND hwndUnderCursor = WindowFromPoint(info->pt);
            HWND topLevel = GetAncestor(hwndUnderCursor, GA_ROOT);
            click.windowId = reinterpret_cast<uintptr_t>(topLevel);

            s_instance->m_mouseHookCallback(click);
        }
    }

    return CallNextHookEx(s_instance->m_mouseHook, nCode, wParam, lParam);
}

bool WindowsPlatformAdapter::startMouseHook(std::function<void(const RecordedClick&)> callback)
{
    if (m_mouseHook) {
        qWarning() << "Mouse hook already active";
        return false;
    }

    m_mouseHookCallback = callback;
    s_instance = this;

    m_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, GetModuleHandle(nullptr), 0);
    if (!m_mouseHook) {
        qWarning() << "Failed to set mouse hook, error:" << GetLastError();
        s_instance = nullptr;
        m_mouseHookCallback = nullptr;
        return false;
    }

    qDebug() << "Mouse hook started";
    return true;
}

void WindowsPlatformAdapter::stopMouseHook()
{
    if (m_mouseHook) {
        UnhookWindowsHookEx(m_mouseHook);
        m_mouseHook = nullptr;
        s_instance = nullptr;
        m_mouseHookCallback = nullptr;
        qDebug() << "Mouse hook stopped";
    }
}

// Additional window operations
uintptr_t WindowsPlatformAdapter::getForegroundWindowId()
{
    return reinterpret_cast<uintptr_t>(GetForegroundWindow());
}

std::wstring WindowsPlatformAdapter::getWindowTitleW(uintptr_t windowId)
{
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    int length = GetWindowTextLengthW(hwnd);
    if (length == 0) return L"";

    std::wstring title;
    title.resize(length + 1);
    GetWindowTextW(hwnd, &title[0], length + 1);
    title.resize(length);  // Remove null terminator
    return title;
}

std::string WindowsPlatformAdapter::getWindowTitle(uintptr_t windowId)
{
    // Legacy ANSI version for compatibility
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    char title[256];
    GetWindowTextA(hwnd, title, sizeof(title));
    return std::string(title);
}

QPoint WindowsPlatformAdapter::getWindowPosition(uintptr_t windowId)
{
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    RECT rect;
    GetWindowRect(hwnd, &rect);
    return QPoint(rect.left, rect.top);
}

WindowInfo WindowsPlatformAdapter::getWindowInfo(uintptr_t windowId)
{
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    WindowInfo info;
    info.id = windowId;

    // Unicode title
    int length = GetWindowTextLengthW(hwnd);
    if (length > 0) {
        std::wstring title;
        title.resize(length + 1);
        GetWindowTextW(hwnd, &title[0], length + 1);
        title.resize(length);
        info.title = title;
    }

    RECT rect;
    GetWindowRect(hwnd, &rect);
    info.x = rect.left;
    info.y = rect.top;
    info.width = rect.right - rect.left;
    info.height = rect.bottom - rect.top;

    // Get process name
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess) {
        WCHAR processName[MAX_PATH];
        GetModuleFileNameExW(hProcess, nullptr, processName, MAX_PATH);
        info.processName = processName;
        CloseHandle(hProcess);
    }

    return info;
}

uintptr_t WindowsPlatformAdapter::getWindowAtPoint(int x, int y)
{
    POINT pt = {x, y};
    HWND hwnd = WindowFromPoint(pt);
    // Return top-level window
    HWND topLevel = GetAncestor(hwnd, GA_ROOT);
    return reinterpret_cast<uintptr_t>(topLevel ? topLevel : hwnd);
}

void WindowsPlatformAdapter::highlightWindow(uintptr_t windowId, int durationMs)
{
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    if (!hwnd) return;

    // Flash window border
    FlashWindow(hwnd, TRUE);

    // Draw highlight rectangle on window
    HDC hdc = GetWindowDC(hwnd);
    if (!hdc) return;

    RECT rect;
    GetWindowRect(hwnd, &rect);

    // Get window dimensions for drawing
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    // Create thick red pen for border
    HPEN hPen = CreatePen(PS_SOLID, 4, RGB(255, 0, 0));
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    // Draw rectangle (in window coordinates, so 0,0 is top-left)
    Rectangle(hdc, 2, 2, width - 2, height - 2);

    // Restore and cleanup after delay using a timer approach
    // We need to keep the highlight visible for durationMs
    QThread::msleep(durationMs);

    // Force window to redraw to remove our highlight
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
    ReleaseDC(hwnd, hdc);

    // Invalidate window to force repaint
    InvalidateRect(hwnd, nullptr, TRUE);
}

ElementInfo WindowsPlatformAdapter::findElementByText(uintptr_t windowId, const std::wstring& text)
{
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    ElementInfo result;
    result.windowId = 0;

    // Context structure for EnumChildWindows callback
    struct FindContext {
        const std::wstring& searchText;
        ElementInfo* result;
    } context{text, &result};

    // Enumerate child windows to find matching element
    EnumChildWindows(hwnd, [](HWND child, LPARAM lParam) -> BOOL {
        auto* ctx = reinterpret_cast<FindContext*>(lParam);

        int length = GetWindowTextLengthW(child);
        if (length > 0) {
            std::wstring windowText;
            windowText.resize(length + 1);
            GetWindowTextW(child, &windowText[0], length + 1);
            windowText.resize(length);

            if (windowText == ctx->searchText ||
                windowText.find(ctx->searchText) != std::wstring::npos) {
                // Found matching element
                RECT rect;
                GetWindowRect(child, &rect);

                // Get parent window position for relative coordinates
                HWND parent = GetParent(child);
                if (!parent) parent = GetAncestor(child, GA_PARENT);
                RECT parentRect;
                GetWindowRect(parent, &parentRect);

                ctx->result->windowId = reinterpret_cast<uintptr_t>(child);
                ctx->result->text = windowText;
                ctx->result->relativeX = rect.left - parentRect.left;
                ctx->result->relativeY = rect.top - parentRect.top;
                ctx->result->width = rect.right - rect.left;
                ctx->result->height = rect.bottom - rect.top;

                WCHAR className[256];
                GetClassNameW(child, className, sizeof(className) / sizeof(WCHAR));
                ctx->result->className = className;

                return FALSE;  // Stop enumeration
            }
        }
        return TRUE;  // Continue enumeration
    }, reinterpret_cast<LPARAM>(&context));

    return result;
}

void WindowsPlatformAdapter::clickElement(uintptr_t elementId, MouseButton button, ClickAction action)
{
    HWND hwnd = reinterpret_cast<HWND>(elementId);
    if (!hwnd) return;

    // Get element dimensions for center click
    RECT rect;
    GetWindowRect(hwnd, &rect);
    int centerX = (rect.right - rect.left) / 2;
    int centerY = (rect.bottom - rect.top) / 2;

    WPARAM wParam = 0;
    UINT downMsg, upMsg;

    switch (button) {
        case MouseButton::Left:
            wParam = MK_LBUTTON;
            downMsg = WM_LBUTTONDOWN;
            upMsg = WM_LBUTTONUP;
            break;
        case MouseButton::Right:
            wParam = MK_RBUTTON;
            downMsg = WM_RBUTTONDOWN;
            upMsg = WM_RBUTTONUP;
            break;
        case MouseButton::Middle:
            wParam = MK_MBUTTON;
            downMsg = WM_MBUTTONDOWN;
            upMsg = WM_MBUTTONUP;
            break;
    }

    LPARAM lParam = MAKELPARAM(centerX, centerY);

    int clickCount = 1;
    switch (action) {
        case ClickAction::Single: clickCount = 1; break;
        case ClickAction::Double: clickCount = 2; break;
        case ClickAction::Triple: clickCount = 3; break;
        case ClickAction::Hold: clickCount = 1; break;
    }

    DWORD_PTR result;
    for (int i = 0; i < clickCount; ++i) {
        SendMessageTimeoutW(hwnd, downMsg, wParam, lParam,
                           SMTO_BLOCK | SMTO_ABORTIFHUNG, 100, &result);
        SendMessageTimeoutW(hwnd, upMsg, 0, lParam,
                           SMTO_BLOCK | SMTO_ABORTIFHUNG, 100, &result);

        if (action == ClickAction::Hold) {
            QThread::msleep(100);
        }

        if (i < clickCount - 1) {
            QThread::msleep(50);
        }
    }
}