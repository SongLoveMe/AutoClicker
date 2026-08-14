#include <QCoreApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>
#include <QDebug>
#include "core/ClickEngine.h"
#include "core/WindowsPlatformAdapter.h"
#include "core/WorkflowStore.h"
#include "core/SequencePointGeometry.h"

#ifdef Q_OS_WIN
namespace {
int minimizedParentClicks = 0;

LRESULT CALLBACK minimizedParentWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_LBUTTONDOWN) ++minimizedParentClicks;
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool minimizedWindowUsesLegacyTopLevelDispatch()
{
    const wchar_t* className = L"AutoClickerMinimizedClickTest";
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = minimizedParentWindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = className;
    if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    HWND parent = CreateWindowExW(0, className, L"", WS_OVERLAPPEDWINDOW,
                                  100, 100, 400, 300, nullptr, nullptr,
                                  windowClass.hInstance, nullptr);
    if (!parent) return false;
    HWND child = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                 0, 0, 300, 220, parent, nullptr,
                                 windowClass.hInstance, nullptr);
    if (!child) { DestroyWindow(parent); return false; }

    ShowWindow(parent, SW_MINIMIZE);
    minimizedParentClicks = 0;
    WindowsPlatformAdapter adapter;
    adapter.simulateClickToWindow(reinterpret_cast<uintptr_t>(parent), 100, 100,
                                  MouseButton::Left, ClickAction::Single);
    const bool clickedParent = minimizedParentClicks == 1;
    DestroyWindow(parent);
    return clickedParent;
}
}
#endif

class FakePlatform final : public PlatformAdapter
{
public:
    WindowInfo target{1, L"Target", L"target.exe", L"TargetWindow", 300, 400, 900, 700};
    WindowInfo stalePlacement{1, L"Target", L"target.exe", L"TargetWindow", 10, 20, 900, 700};
    uintptr_t foreground = 1;
    bool minimized = false;
    bool minimizedRect = false;
    QPoint lastClick{-1, -1};
    int clickCount = 0;
    int backgroundClickCount = 0;

    void simulateClick(int x, int y, MouseButton, ClickAction, int) override { lastClick = QPoint(x, y); ++clickCount; }
    void simulateClickNoInterference(int x, int y, MouseButton button, ClickAction action, ClickMethod) override { simulateClick(x, y, button, action, 100); }
    void simulateClickToWindow(uintptr_t, int x, int y, MouseButton button, ClickAction action) override { lastClick = QPoint(target.x + x, target.y + y); ++clickCount; ++backgroundClickCount; }
    void simulateMouseMove(int, int) override {}
    QPoint getMousePosition() override { return QPoint(100, 100); }
    bool registerHotkey(int, int, int, std::function<void()>) override { return true; }
    bool unregisterHotkey(int) override { return true; }
    bool handleHotkeyEvent(int) override { return true; }
    WindowInfo getActiveWindow() override { return target; }
    std::vector<WindowInfo> listWindows() override { return {target}; }
    bool setForegroundWindow(uintptr_t) override { return true; }
    uintptr_t getForegroundWindowId() override { return foreground; }
    std::wstring getWindowTitleW(uintptr_t) override { return target.title; }
    std::string getWindowTitle(uintptr_t) override { return "Target"; }
    QPoint getWindowPosition(uintptr_t) override { return QPoint(target.x, target.y); }
    WindowInfo getWindowInfo(uintptr_t id) override { WindowInfo info = target; info.id = id; if (minimized && minimizedRect) { info.x = -32000; info.y = -32000; info.width = 160; info.height = 30; } return info; }
    uintptr_t findWindowByLocator(const std::wstring&, const std::wstring&, const std::wstring&) override { return target.id; }
    bool isWindowMinimized(uintptr_t) override { return minimized; }
    WindowInfo getWindowPlacementInfo(uintptr_t id) override { WindowInfo info = stalePlacement; info.id = id; return info; }
    uintptr_t getWindowAtPoint(int, int) override { return target.id; }
    void highlightWindow(uintptr_t, int) override {}
    bool startMouseHook(std::function<void(const RecordedClick&)>) override { return true; }
    void stopMouseHook() override {}
    ElementInfo findElementByText(uintptr_t, const std::wstring&) override { return {}; }
    void clickElement(uintptr_t, MouseButton, ClickAction) override {}
    QImage captureWindow(uintptr_t) override { return QImage(10, 10, QImage::Format_ARGB32); }
};

static bool waitFor(int milliseconds)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
    return true;
}

static Workflow makeWorkflow()
{
    Workflow workflow;
    workflow.targetWindow = {QStringLiteral("Target"), QStringLiteral("target.exe"), QStringLiteral("TargetWindow")};
    workflow.clickMethod = ClickMethod::SimulateMouse;
    WorkflowSequence sequence{QStringLiteral("A")};
    sequence.loopCount = 1;
    SequencePoint point{};
    point.screenX = 305; point.screenY = 406; point.relX = 5; point.relY = 6; point.intervalMs = 1; point.boundWindowWidth = 900; point.boundWindowHeight = 700;
    point.windowId = 1; point.windowTitle = L"Target"; point.processName = L"target.exe"; point.windowClass = L"TargetWindow"; point.hasWindowBinding = true;
    sequence.points.append(point);
    workflow.sequences.append(sequence);
    return workflow;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
#ifdef Q_OS_WIN
    if (!minimizedWindowUsesLegacyTopLevelDispatch()) {
        qCritical() << "Minimized no-interference click was not sent to the bound top-level window";
        return 1;
    }
#endif
    SequencePoint geometryPoint{}; geometryPoint.screenX = 600; geometryPoint.screenY = 500; geometryPoint.relX = 500; geometryPoint.relY = 400; geometryPoint.boundWindowWidth = 1000; geometryPoint.boundWindowHeight = 800; geometryPoint.windowId = 1; geometryPoint.windowTitle = L"Target"; geometryPoint.processName = L"target.exe"; geometryPoint.windowClass = L"TargetWindow"; geometryPoint.hasWindowBinding = true;
    WindowInfo movedWindow{1, L"Target", L"target.exe", L"TargetWindow", 200, 150, 1000, 800};
    const ResolvedPointGeometry moved = resolveSequencePoint(geometryPoint, &movedWindow);
    if (!moved.windowFound || moved.globalX != 700 || moved.globalY != 550 || moved.referenceRelX != 500 || moved.referenceRelY != 400) { qCritical() << "Moved-window coordinate resolution failed"; return 1; }
    movedWindow.width = 2000; movedWindow.height = 1600;
    const ResolvedPointGeometry scaled = resolveSequencePoint(geometryPoint, &movedWindow);
    if (scaled.globalX != 1200 || scaled.globalY != 950 || scaled.currentRelX != 1000 || scaled.currentRelY != 800 || scaled.referenceRelX != 500 || scaled.referenceRelY != 400) { qCritical() << "Scaled-window coordinate resolution failed"; return 1; }
    const ResolvedPointGeometry unbound = resolveSequencePoint(SequencePoint{600, 500, RecordedButton::Left, 0, 0, 0}, nullptr);
    if (unbound.hasBinding || unbound.globalX != 600 || unbound.globalY != 500) { qCritical() << "Unbound coordinate resolution failed"; return 1; }
    auto platform = std::make_shared<FakePlatform>();
    ClickEngine engine(platform);
    engine.setWorkflow(makeWorkflow(), 1);
    engine.start();
    waitFor(30);
    if (platform->clickCount != 1 || platform->lastClick != QPoint(305, 406)) {
        qCritical() << "Bound point did not use the current window rectangle" << platform->lastClick;
        return 1;
    }

    platform->foreground = 2;
    engine.setWorkflow(makeWorkflow(), 1);
    engine.start();
    waitFor(30);
    if (engine.isRunning() || platform->clickCount != 1) {
        qCritical() << "Foreground loss did not stop the workflow";
        return 1;
    }

    platform->foreground = 2;
    platform->minimized = true;
    platform->minimizedRect = true;
    platform->clickCount = 0;
    platform->backgroundClickCount = 0;
    Workflow background = makeWorkflow();
    background.clickMethod = ClickMethod::NoInterference;
    engine.setWorkflow(background, 1);
    engine.start();
    waitFor(30);
    if (engine.isRunning() || platform->clickCount != 1 || platform->backgroundClickCount != 1 || platform->lastClick.x() < 0 || platform->lastClick.y() < 0) {
        qCritical() << "No-interference mode did not click a background/minimized window";
        return 1;
    }

    // A normal but non-foreground window must use the same background path;
    // it must not fall back to foreground/physical mouse injection.
    platform->foreground = 2;
    platform->minimized = false;
    platform->minimizedRect = false;
    platform->clickCount = 0;
    platform->backgroundClickCount = 0;
    engine.setWorkflow(background, 1);
    engine.start();
    waitFor(30);
    if (engine.isRunning() || platform->clickCount != 1 || platform->backgroundClickCount != 1) {
        qCritical() << "No-interference mode did not click a normal background window";
        return 1;
    }

    Workflow rejected = makeWorkflow();
    rejected.clickMethod = ClickMethod::NoInterference;
    rejected.sequences[0].transition.enabled = true;
    rejected.sequences[0].transition.type = VisualConditionType::Template;
    rejected.sequences[0].transition.targetSequenceIndex = 1;
    rejected.sequences.append(WorkflowSequence{QStringLiteral("B")});
    bool rejectedVisual = false;
    QObject::connect(&engine, &ClickEngine::errorOccurred, [&rejectedVisual](const QString& message) {
        rejectedVisual = message.contains(QStringLiteral("不支持视觉条件"));
    });
    engine.setWorkflow(rejected, 1);
    engine.start();
    waitFor(10);
    if (engine.isRunning() || !rejectedVisual) {
        qCritical() << "No-interference mode did not reject visual conditions";
        return 1;
    }

    platform->foreground = 1;
    platform->minimized = false;
    platform->minimizedRect = false;
    Workflow chained = makeWorkflow();
    chained.sequences[0].transition.enabled = true;
    chained.sequences[0].transition.type = VisualConditionType::LoopEnd;
    chained.sequences[0].transition.targetSequenceIndex = 1;
    WorkflowSequence second{QStringLiteral("B")};
    second.loopCount = 1;
    second.points = chained.sequences[0].points;
    chained.sequences.append(second);
    bool completed = false;
    QObject::connect(&engine, &ClickEngine::workflowCompleted, [&completed](const QString&) { completed = true; });
    platform->clickCount = 0;
    engine.setWorkflow(chained, 1);
    engine.start();
    waitFor(60);
    if (!completed || engine.isRunning() || platform->clickCount != 2) {
        qCritical() << "Loop-end transition did not complete the chained workflow" << completed << platform->clickCount;
        return 1;
    }

    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("workflow.acworkflow"));
    Workflow workflow = makeWorkflow(); workflow.clickMethod = ClickMethod::NoInterference; workflow.sequences[0].points[0].clickCount = 2; workflow.sequences[0].points[0].clickIntervalMs = 7; workflow.sequences[0].transition.type = VisualConditionType::LoopEnd;
    QString error;
    if (!WorkflowStore::save(workflow, path, &error)) { qCritical() << error; return 1; }
    Workflow loaded;
    if (!WorkflowStore::load(path, &loaded, &error)) { qCritical() << error; return 1; }
    const SequencePoint& savedPoint = loaded.sequences[0].points[0];
    if (loaded.clickMethod != ClickMethod::NoInterference || loaded.targetWindow.className != QStringLiteral("TargetWindow") || loaded.sequences[0].transition.type != VisualConditionType::LoopEnd || savedPoint.clickCount != 2 || savedPoint.clickIntervalMs != 7 || savedPoint.boundWindowWidth != 900) {
        qCritical() << "Workflow storage lost click method, window class, or loop-end transition" << error;
        return 1;
    }
    return 0;
}
