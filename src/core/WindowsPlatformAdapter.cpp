#include "WindowsPlatformAdapter.h"
#include <QApplication>
#include <QGuiApplication>
#include <QDebug>
#include <QThread>
#include <QScreen>

namespace {
// This uses child client rectangles rather than the parent window's visible
// rectangle, which becomes an icon-sized placeholder while a window is
// minimized. The point is updated into the deepest matched child's space.
HWND findLogicalChildAtPoint(HWND parent, POINT* point)
{
    for (HWND child = GetWindow(parent, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        POINT candidate = *point;
        SetLastError(ERROR_SUCCESS);
        if (MapWindowPoints(parent, child, &candidate, 1) == 0 && GetLastError() != ERROR_SUCCESS) continue;

        RECT clientRect{};
        if (!GetClientRect(child, &clientRect) || !PtInRect(&clientRect, candidate)) continue;

        *point = candidate;
        return findLogicalChildAtPoint(child, point);
    }
    return parent;
}
}

WindowsPlatformAdapter::WindowsPlatformAdapter()
    : m_hwnd(nullptr)
{
}

WindowsPlatformAdapter::~WindowsPlatformAdapter()
{
    stopMouseHook();

    m_hotkeyBindings.clear();
    if (m_keyboardHook) {
        if (!UnhookWindowsHookEx(m_keyboardHook)) {
            qWarning() << "Failed to remove keyboard hook, error=" << GetLastError();
        }
        m_keyboardHook = nullptr;
    }
    if (s_keyboardInstance == this) {
        s_keyboardInstance = nullptr;
    }
}

// v2.2: Non-blocking wait that processes Qt events during delay
// This allows hotkeys to be processed while waiting for click timing
void WindowsPlatformAdapter::nonBlockingWait(int ms)
{
    if (ms <= 0) return;
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
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
            nonBlockingWait(holdDuration);  // v2.2: non-blocking wait
        } else {
            nonBlockingWait(10);  // v2.2: non-blocking wait
        }

        // Mouse up
        INPUT inputUp = {};
        inputUp.type = INPUT_MOUSE;
        inputUp.mi.dwFlags = upFlag;
        SendInput(1, &inputUp, sizeof(INPUT));

        if (i < clickCount - 1) {
            nonBlockingWait(50);  // v2.2: non-blocking wait between clicks
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
    POINT targetPoint = clientPt;
    HWND targetWindow = findLogicalChildAtPoint(topLevelWindow, &targetPoint);

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

    LPARAM lParam = MAKELPARAM(targetPoint.x, targetPoint.y);

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
    SendMessageTimeoutW(targetWindow, WM_MOUSEMOVE, 0, lParam,
                       SMTO_ABORTIFHUNG, 100, &result);

    for (int i = 0; i < clickCount; ++i) {
        SendMessageTimeoutW(targetWindow, downMsg, wParam, lParam,
                           SMTO_ABORTIFHUNG, 100, &result);
        SendMessageTimeoutW(targetWindow, upMsg, 0, lParam,
                           SMTO_ABORTIFHUNG, 100, &result);

        if (i < clickCount - 1) {
            nonBlockingWait(50);  // v2.2: non-blocking wait for hotkey responsiveness
        }
    }

    // For double-click, also send WM_LBUTTONDBLCLK
    if (action == ClickAction::Double && button == MouseButton::Left) {
        SendMessageTimeoutW(targetWindow, WM_LBUTTONDBLCLK, wParam, lParam,
                           SMTO_ABORTIFHUNG, 100, &result);
    }
}

void WindowsPlatformAdapter::simulateClickToWindow(uintptr_t windowId, int relX, int relY, MouseButton button, ClickAction action)
{
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    if (!hwnd || !IsWindow(hwnd)) return;

    const bool minimized = IsIconic(hwnd) != 0;
    HWND target = hwnd;
    POINT targetPoint{relX, relY};
    if (!minimized) {
        // The engine stores coordinates relative to the top-level window
        // frame. Normal background windows need client conversion and child
        // dispatch for controls such as browser renderers.
        RECT windowRect{};
        if (!GetWindowRect(hwnd, &windowRect)) return;
        POINT clientOrigin{0, 0};
        if (!ClientToScreen(hwnd, &clientOrigin)) return;
        targetPoint = {
            relX + (clientOrigin.x - windowRect.left),
            relY + (clientOrigin.y - windowRect.top)
        };
        target = findLogicalChildAtPoint(hwnd, &targetPoint);
    }

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

    LPARAM lParam = MAKELPARAM(targetPoint.x, targetPoint.y);

    int clickCount = 1;
    switch (action) {
        case ClickAction::Single: clickCount = 1; break;
        case ClickAction::Double: clickCount = 2; break;
        case ClickAction::Triple: clickCount = 3; break;
        case ClickAction::Hold: clickCount = 1; break;
    }

    // Use SendMessageTimeout for synchronous delivery. WM_MOUSEMOVE gives
    // controls such as browser renderers the same target tracking context as
    // a real pointer without moving the user's cursor or activating a window.
    DWORD_PTR result;
    if (!minimized) {
        SendMessageTimeoutW(target, WM_MOUSEMOVE, 0, lParam, SMTO_ABORTIFHUNG, 100, &result);
    }

    for (int i = 0; i < clickCount; ++i) {
        SendMessageTimeoutW(target, downMsg, wParam, lParam,
                           SMTO_ABORTIFHUNG, 100, &result);
        SendMessageTimeoutW(target, upMsg, 0, lParam,
                           SMTO_ABORTIFHUNG, 100, &result);

        if (action == ClickAction::Hold) {
            nonBlockingWait(100);  // v2.2: non-blocking wait
        }

        if (i < clickCount - 1) {
            nonBlockingWait(50);  // v2.2: non-blocking wait
        }
    }

    // For double-click, also send WM_LBUTTONDBLCLK
    if (action == ClickAction::Double && button == MouseButton::Left) {
        SendMessageTimeoutW(target, WM_LBUTTONDBLCLK, wParam, lParam,
                           SMTO_ABORTIFHUNG, 100, &result);
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
    if (id < 0 || !callback) {
        qWarning() << "Cannot register hotkey: invalid id or callback";
        return false;
    }

    const int virtualKey = mapKeyToVirtualKey(key);
    const UINT modifierFlags = static_cast<UINT>(mapModifiersToModFlags(modifiers));
    if (virtualKey <= 0) {
        qWarning() << "Cannot register hotkey: unsupported key=" << key;
        return false;
    }

    if (s_keyboardInstance && s_keyboardInstance != this) {
        qWarning() << "Cannot register hotkey: another adapter owns the global keyboard hook";
        return false;
    }

    if (!m_keyboardHook) {
        HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc,
                                       GetModuleHandleW(nullptr), 0);
        if (!hook) {
            qWarning() << "Failed to install keyboard hook, error=" << GetLastError();
            return false;
        }
        m_keyboardHook = hook;
        s_keyboardInstance = this;
    }

    HotkeyBinding binding;
    binding.virtualKey = virtualKey;
    binding.modifiers = modifierFlags;
    binding.callback = std::move(callback);
    m_hotkeyBindings.insert(id, std::move(binding));
    qDebug() << "Global hotkey registered: id=" << id << "key=" << key;
    return true;
}

bool WindowsPlatformAdapter::unregisterHotkey(int id)
{
    if (!m_hotkeyBindings.contains(id)) {
        return false;
    }

    m_hotkeyBindings.remove(id);
    if (m_hotkeyBindings.isEmpty() && m_keyboardHook) {
        if (!UnhookWindowsHookEx(m_keyboardHook)) {
            qWarning() << "Failed to remove keyboard hook, error=" << GetLastError();
            return false;
        }
        m_keyboardHook = nullptr;
        if (s_keyboardInstance == this) {
            s_keyboardInstance = nullptr;
        }
    }
    return true;
}

bool WindowsPlatformAdapter::handleHotkeyEvent(int id)
{
    auto it = m_hotkeyBindings.constFind(id);
    if (it == m_hotkeyBindings.constEnd()) {
        return false;
    }
    it->callback();
    return true;
}

UINT WindowsPlatformAdapter::getCurrentModifierFlags() const
{
    UINT modifiers = 0;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) modifiers |= MOD_CONTROL;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) modifiers |= MOD_SHIFT;
    if (GetAsyncKeyState(VK_MENU) & 0x8000) modifiers |= MOD_ALT;
    if (GetAsyncKeyState(VK_LWIN) & 0x8000 || GetAsyncKeyState(VK_RWIN) & 0x8000) modifiers |= MOD_WIN;
    return modifiers;
}

bool WindowsPlatformAdapter::matchesHotkey(const HotkeyBinding& binding, DWORD virtualKey) const
{
    return binding.virtualKey == static_cast<int>(virtualKey)
        && binding.modifiers == getCurrentModifierFlags();
}

LRESULT CALLBACK WindowsPlatformAdapter::KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0 || !s_keyboardInstance) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    if (!info) {
        return CallNextHookEx(s_keyboardInstance->m_keyboardHook, nCode, wParam, lParam);
    }

    const bool keyDown = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
    const bool keyUp = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
    if (!keyDown && !keyUp) {
        return CallNextHookEx(s_keyboardInstance->m_keyboardHook, nCode, wParam, lParam);
    }

    for (auto it = s_keyboardInstance->m_hotkeyBindings.begin();
         it != s_keyboardInstance->m_hotkeyBindings.end(); ++it) {
        HotkeyBinding& binding = it.value();
        if (binding.virtualKey != static_cast<int>(info->vkCode)) {
            continue;
        }

        if (keyDown) {
            if (binding.pressed) {
                // Swallow auto-repeat events for a hotkey that was already consumed.
                return 1;
            }
            if (!s_keyboardInstance->matchesHotkey(binding, info->vkCode)) {
                continue;
            }
            binding.pressed = true;
            binding.callback();
            return 1;
        }

        if (binding.pressed) {
            // Always consume the matching key-up, even if a modifier was released first.
            binding.pressed = false;
            return 1;
        }
    }

    return CallNextHookEx(s_keyboardInstance->m_keyboardHook, nCode, wParam, lParam);
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
            WCHAR className[256]{};
            GetClassNameW(hwnd, className, 256);
            const bool isSystemSurface = wcscmp(className, L"Progman") == 0 || wcscmp(className, L"WorkerW") == 0 || wcscmp(className, L"Shell_TrayWnd") == 0;
            if (length > 0 || isSystemSurface) {
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

                info.className = className;

                list->push_back(info);
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&windows));

    const auto addIfMissing = [&windows, this](HWND hwnd) {
        if (!hwnd) return;
        const uintptr_t id = reinterpret_cast<uintptr_t>(hwnd);
        for (const WindowInfo& entry : windows) if (entry.id == id) return;
        windows.push_back(getWindowInfo(id));
    };
    addIfMissing(FindWindowW(L"Progman", nullptr));
    addIfMissing(FindWindowW(L"Shell_TrayWnd", nullptr));
    addIfMissing(GetDesktopWindow());

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
WindowsPlatformAdapter* WindowsPlatformAdapter::s_keyboardInstance = nullptr;
WindowsPlatformAdapter* WindowsPlatformAdapter::s_instance = nullptr;

bool WindowsPlatformAdapter::shouldFilterClick(int x, int y)
{
    // Use process ID comparison - more reliable than HWND comparison for Qt windows
    DWORD myProcessId = GetCurrentProcessId();

    POINT pt = {x, y};
    HWND clickedWindow = WindowFromPoint(pt);
    if (!clickedWindow) return false;

    // Get the process ID of the clicked window
    DWORD clickedProcessId = 0;
    GetWindowThreadProcessId(clickedWindow, &clickedProcessId);

    // Filter if clicked window belongs to our process (AutoClicker)
    return (clickedProcessId == myProcessId);
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

    // Initialize m_hwnd if not already set (ensure shouldFilterClick works)
    if (!m_hwnd) {
        QWidget* mainWindow = QApplication::topLevelWidgets().isEmpty() ? nullptr : QApplication::topLevelWidgets().first();
        if (mainWindow) {
            m_hwnd = reinterpret_cast<HWND>(mainWindow->winId());
            qDebug() << "Window handle initialized for mouse hook filtering";
        }
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

    WCHAR className[256]{};
    GetClassNameW(hwnd, className, 256);
    info.className = className;

    return info;
}

uintptr_t WindowsPlatformAdapter::findWindowByLocator(const std::wstring& title, const std::wstring& processName,
                                                       const std::wstring& className)
{
    const auto matches = [&](HWND hwnd) {
        if (!IsWindow(hwnd)) return false;
        WCHAR classBuffer[256]{};
        GetClassNameW(hwnd, classBuffer, 256);
        if (!className.empty() && className != classBuffer) return false;

        const std::wstring windowTitle = getWindowTitleW(reinterpret_cast<uintptr_t>(hwnd));
        if (!title.empty() && title != windowTitle) return false;

        if (!processName.empty()) {
            const WindowInfo info = getWindowInfo(reinterpret_cast<uintptr_t>(hwnd));
            if (info.processName != processName) return false;
        }
        return !title.empty() || !processName.empty() || !className.empty();
    };

    HWND desktop = GetDesktopWindow();
    if (matches(desktop)) return reinterpret_cast<uintptr_t>(desktop);

    struct SearchContext { const decltype(matches)& predicate; HWND result = nullptr; } context{matches};
    EnumWindows([](HWND hwnd, LPARAM value) -> BOOL {
        auto* context = reinterpret_cast<SearchContext*>(value);
        if (context->predicate(hwnd)) { context->result = hwnd; return FALSE; }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context));
    return reinterpret_cast<uintptr_t>(context.result);
}

bool WindowsPlatformAdapter::isWindowMinimized(uintptr_t windowId)
{
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    return IsIconic(hwnd) != 0;
}

WindowInfo WindowsPlatformAdapter::getWindowPlacementInfo(uintptr_t windowId)
{
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    WindowInfo info;  // Default values are 0 due to struct initialization

    // Check if window is valid
    if (!IsWindow(hwnd)) {
        info.id = 0;  // Mark as invalid
        return info;
    }

    info.id = windowId;

    // Use GetWindowPlacement for proper handling of minimized/maximized windows
    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);

    if (GetWindowPlacement(hwnd, &wp)) {
        // rcNormalPosition gives the "restored" position
        // This works correctly even when window is minimized/maximized
        info.x = wp.rcNormalPosition.left;
        info.y = wp.rcNormalPosition.top;
        info.width = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
        info.height = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
    }

    // Get title (Unicode)
    int length = GetWindowTextLengthW(hwnd);
    if (length > 0) {
        std::wstring title;
        title.resize(length + 1);
        GetWindowTextW(hwnd, &title[0], length + 1);
        title.resize(length);
        info.title = title;
    }

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

    WCHAR className[256]{};
    GetClassNameW(hwnd, className, 256);
    info.className = className;

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
    nonBlockingWait(durationMs);  // v2.2: non-blocking wait

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
                           SMTO_ABORTIFHUNG, 100, &result);
        SendMessageTimeoutW(hwnd, upMsg, 0, lParam,
                           SMTO_ABORTIFHUNG, 100, &result);

        if (action == ClickAction::Hold) {
            nonBlockingWait(100);  // v2.2: non-blocking wait
        }

        if (i < clickCount - 1) {
            nonBlockingWait(50);  // v2.2: non-blocking wait
        }
    }
}

QImage WindowsPlatformAdapter::captureWindow(uintptr_t windowId)
{
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    if (!hwnd || !IsWindow(hwnd) || IsIconic(hwnd) || GetForegroundWindow() != hwnd) {
        return QImage();
    }

    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) return QImage();
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) return QImage();

    HDC windowDc = GetWindowDC(hwnd);
    HDC memoryDc = CreateCompatibleDC(windowDc);
    HBITMAP bitmap = CreateCompatibleBitmap(windowDc, width, height);
    HGDIOBJ previous = SelectObject(memoryDc, bitmap);
    const bool copied = BitBlt(memoryDc, 0, 0, width, height, windowDc, 0, 0, SRCCOPY | CAPTUREBLT);

    QImage image;
    if (copied) {
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        image = QImage(width, height, QImage::Format_ARGB32);
        if (GetDIBits(memoryDc, bitmap, 0, height, image.bits(), &info, DIB_RGB_COLORS) == 0) image = QImage();
    }

    SelectObject(memoryDc, previous);
    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    ReleaseDC(hwnd, windowDc);
    return image;
}
