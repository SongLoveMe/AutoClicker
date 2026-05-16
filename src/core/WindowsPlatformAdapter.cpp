#include "WindowsPlatformAdapter.h"
#include <QApplication>
#include <QGuiApplication>
#include <QDebug>

WindowsPlatformAdapter::WindowsPlatformAdapter()
    : m_hwnd(nullptr)
{
}

WindowsPlatformAdapter::~WindowsPlatformAdapter()
{
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
    WindowInfo info;
    info.id = reinterpret_cast<uintptr_t>(hwnd);

    char title[256];
    GetWindowTextA(hwnd, title, sizeof(title));
    info.title = title;

    RECT rect;
    GetWindowRect(hwnd, &rect);
    info.x = rect.left;
    info.y = rect.top;
    info.width = rect.right - rect.left;
    info.height = rect.bottom - rect.top;

    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess) {
        char processName[MAX_PATH];
        GetModuleFileNameExA(hProcess, nullptr, processName, MAX_PATH);
        info.processName = processName;
        CloseHandle(hProcess);
    }

    return info;
}

std::vector<WindowInfo> WindowsPlatformAdapter::listWindows()
{
    std::vector<WindowInfo> windows;

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* list = reinterpret_cast<std::vector<WindowInfo>*>(lParam);

        if (IsWindowVisible(hwnd)) {
            WindowInfo info;
            info.id = reinterpret_cast<uintptr_t>(hwnd);

            char title[256];
            GetWindowTextA(hwnd, title, sizeof(title));
            if (strlen(title) > 0) {
                info.title = title;

                RECT rect;
                GetWindowRect(hwnd, &rect);
                info.x = rect.left;
                info.y = rect.top;
                info.width = rect.right - rect.left;
                info.height = rect.bottom - rect.top;

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