#include "ClickEngine.h"
#include "SequencePointGeometry.h"
#include <QRandomGenerator>
#include <QDebug>
#include "VisualDetector.h"

ClickEngine::ClickEngine(std::shared_ptr<PlatformAdapter> platform, QObject* parent)
    : QObject(parent)
    , m_platform(platform)
    , m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &ClickEngine::performClick);
}

ClickEngine::~ClickEngine()
{
    stop();
}

void ClickEngine::setMode(ClickMode mode)
{
    m_mode = mode;
}

void ClickEngine::setPosition(int x, int y)
{
    m_targetX = x;
    m_targetY = y;
}

void ClickEngine::setSequence(const QList<SequencePoint>& points)
{
    m_sequencePoints = points;
    m_sequenceIndex = 0;
}

void ClickEngine::setButton(MouseButton button)
{
    m_button = button;
}

void ClickEngine::setAction(ClickAction action)
{
    m_action = action;
}

void ClickEngine::setInterval(int baseMs, int jitterMs)
{
    m_intervalBase = baseMs;
    m_jitterRange = jitterMs;
}

void ClickEngine::setClickCount(int count)
{
    m_clickCount = count;
}

void ClickEngine::setAntiDetect(bool enabled)
{
    m_antiDetect = enabled;
}

void ClickEngine::setClickMethod(ClickMethod method)
{
    m_clickMethod = method;
}

void ClickEngine::setTargetWindow(uintptr_t windowId)
{
    m_targetWindowId = windowId;
}

void ClickEngine::setTargetElement(const ElementInfo& element)
{
    m_targetElement = element;
    m_hasTargetElement = (element.windowId != 0);
}

void ClickEngine::setAutoStopOnWindowLost(bool enabled)
{
    m_autoStopOnWindowLost = enabled;
}

void ClickEngine::setWorkflow(const Workflow& workflow, uintptr_t windowId, int startSequenceIndex)
{
    m_workflow = workflow;
    m_workflowWindowId = windowId;
    m_hasWorkflow = !workflow.sequences.isEmpty();
    m_workflowSequenceIndex = qBound(0, startSequenceIndex, qMax(0, workflow.sequences.size() - 1));
    m_workflowPointIndex = 0;
    m_workflowPointClickIndex = 0;
    m_workflowCompletedLoops = 0;
    m_workflowConsecutiveHits = 0;
    m_clickMethod = workflow.clickMethod;
}

void ClickEngine::stopWorkflowWithReason(const QString& reason)
{
    emit workflowFinished(reason);
    stop();
}

void ClickEngine::start()
{
    if (m_running) {
        return;
    }

    if (m_hasWorkflow && m_clickMethod == ClickMethod::NoInterference && workflowHasVisualConditions()) {
        emit errorOccurred(QStringLiteral("无干扰后台模式不支持视觉条件，请改用模拟鼠标或移除视觉条件。"));
        return;
    }

    m_running = true;
    m_paused = false;
    m_totalClicks = 0;
    m_currentCount = 0;
    m_sequenceIndex = 0;
    // Keep the sequence selected by the user when this workflow execution begins.
    m_workflowPointIndex = 0;
    m_workflowPointClickIndex = 0;
    m_workflowCompletedLoops = 0;
    m_workflowConsecutiveHits = 0;
    if (m_hasWorkflow) {
        if (m_workflow.sequences.isEmpty()) {
            emit errorOccurred(QStringLiteral("工作流没有序列"));
            m_running = false;
            return;
        }
        emit workflowSequenceChanged(m_workflow.sequences[m_workflowSequenceIndex].name, m_workflowSequenceIndex, 0);
        QTimer::singleShot(0, this, &ClickEngine::performWorkflowClick);
        return;
    }

    // Start timer with first interval
    int interval = getRandomInterval();
    m_timer->start(interval);

    qDebug() << "ClickEngine started, interval:" << interval << "ms";
}

void ClickEngine::stop()
{
    if (!m_running) {
        return;
    }

    m_timer->stop();
    m_running = false;
    m_paused = false;
    m_currentTargetWindowId = 0;

    qDebug() << "ClickEngine stopped, total clicks:" << m_totalClicks;
    emit finished();
}

void ClickEngine::pause()
{
    if (m_running && !m_paused) {
        m_timer->stop();
        m_paused = true;
        qDebug() << "ClickEngine paused";
    }
}

void ClickEngine::resume()
{
    if (m_running && m_paused) {
        m_paused = false;
        if (m_hasWorkflow) {
            QTimer::singleShot(0, this, &ClickEngine::performWorkflowClick);
            return;
        }
        int interval = getRandomInterval();
        m_timer->start(interval);
        qDebug() << "ClickEngine resumed, interval:" << interval << "ms";
    }
}

bool ClickEngine::isRunning() const
{
    return m_running;
}

bool ClickEngine::isPaused() const
{
    return m_paused;
}

bool ClickEngine::workflowHasVisualConditions() const
{
    for (const WorkflowSequence& sequence : m_workflow.sequences) {
        if (sequence.transition.enabled
            && sequence.transition.type != VisualConditionType::LoopEnd) {
            return true;
        }
    }
    return false;
}

int ClickEngine::getTotalClicks() const
{
    return m_totalClicks;
}

uintptr_t ClickEngine::getCurrentTargetWindowId() const
{
    return m_currentTargetWindowId;
}

void ClickEngine::performClick()
{
    if (m_hasWorkflow) { performWorkflowClick(); return; }
    // Stop timer during click execution
    m_timer->stop();

    // Check if we've reached the click count limit
    // -1 means infinite, 0 means no clicks (disabled), >0 means specific count
    if (m_clickCount >= 0 && m_currentCount >= m_clickCount) {
        stop();
        return;
    }

    // Check running state (may have been stopped during async wait)
    if (!m_running || m_paused) {
        return;
    }

    // Determine click position based on mode
    int clickX = m_targetX;
    int clickY = m_targetY;
    uintptr_t clickWindowId = m_targetWindowId;
    MouseButton clickButton = m_button;
    int nextInterval = getRandomInterval();  // Default interval

    // If we have a target element, use its position
    if (m_hasTargetElement) {
        if (m_targetElement.windowId != 0) {
            QPoint windowPos = m_platform->getWindowPosition(m_targetElement.windowId);
            clickX = windowPos.x() + m_targetElement.relativeX + m_targetElement.width / 2;
            clickY = windowPos.y() + m_targetElement.relativeY + m_targetElement.height / 2;
            clickWindowId = m_targetElement.windowId;
        }
        if (m_antiDetect) {
            QPoint offset = getRandomOffset();
            clickX += offset.x();
            clickY += offset.y();
        }
    } else {
        // Use standard position calculation based on mode
        switch (m_mode) {
            case ClickMode::FixedPosition:
                // Check if we should use window-relative coordinates
                if (m_targetWindowId != 0) {
                    QPoint windowPos = m_platform->getWindowPosition(m_targetWindowId);
                    clickX = windowPos.x() + m_targetX;
                    clickY = windowPos.y() + m_targetY;
                    clickWindowId = m_targetWindowId;
                }
                if (m_antiDetect) {
                    QPoint offset = getRandomOffset();
                    clickX += offset.x();
                    clickY += offset.y();
                }
                break;

            case ClickMode::FollowCursor:
                if (m_platform) {
                    QPoint pos = m_platform->getMousePosition();
                    clickX = pos.x();
                    clickY = pos.y();
                    clickWindowId = m_platform->getWindowAtPoint(clickX, clickY);
                    if (m_antiDetect) {
                        QPoint offset = getRandomOffset();
                        clickX += offset.x();
                        clickY += offset.y();
                    }
                }
                break;

            case ClickMode::Sequence:
                if (m_sequencePoints.isEmpty()) {
                    qWarning() << "Sequence mode but no points defined";
                    stop();
                    return;
                }
                {
                    SequencePoint pt = getNextSequencePoint();
                    clickButton = toMouseButton(pt.button);  // Use per-point button

                    // v2.2: Use per-point interval if specified
                    if (pt.intervalMs > 0) {
                        nextInterval = pt.intervalMs;
                    }

                    // Use per-point window binding
                    if (pointHasWindowBinding(pt)) {
                        uintptr_t resolvedId = m_platform->findWindowByLocator(pt.windowTitle, pt.processName, pt.windowClass);
                        WindowInfo winInfo = resolvedId ? (m_platform->isWindowMinimized(resolvedId) ? m_platform->getWindowPlacementInfo(resolvedId) : m_platform->getWindowInfo(resolvedId)) : m_platform->getWindowPlacementInfo(pt.windowId);
                        if (winInfo.id && (resolvedId || winInfo.id == pt.windowId || winInfo.title == pt.windowTitle)) {
                            const ResolvedPointGeometry resolved = resolveSequencePoint(pt, &winInfo);
                            clickX = resolved.globalX;
                            clickY = resolved.globalY;
                            clickWindowId = winInfo.id;
                        } else {
                            // v2.2: Window has been closed - emit signal and optionally stop
                            QString windowTitleStr = QString::fromWCharArray(pt.windowTitle.c_str());
                            emit windowLost(pt.windowId, windowTitleStr);

                            if (m_autoStopOnWindowLost) {
                                stop();
                                emit errorOccurred(QString("窗口已关闭: %1").arg(windowTitleStr));
                                return;
                            }

                            // If not auto-stopping, fall back to absolute coordinates
                            clickX = pt.screenX;
                            clickY = pt.screenY;
                            clickWindowId = 0;
                            qDebug() << "Window binding lost, using absolute coords";
                        }
                    } else {
                        clickX = pt.screenX;
                        clickY = pt.screenY;
                    }

                    if (m_antiDetect) {
                        QPoint offset = getRandomOffset();
                        clickX += offset.x();
                        clickY += offset.y();
                    }
                }
                break;

            case ClickMode::RandomArea:
                qWarning() << "Random area mode not implemented yet";
                break;

            case ClickMode::Drag:
                qWarning() << "Drag mode not implemented yet";
                break;
        }
    }

    // Update current target window for UI display
    m_currentTargetWindowId = clickWindowId;

    // Perform the click
    if (m_platform) {
        if (m_hasTargetElement && m_clickMethod == ClickMethod::NoInterference) {
            m_platform->clickElement(m_targetElement.windowId, clickButton, m_action);
        } else if (m_clickMethod == ClickMethod::NoInterference && clickWindowId != 0) {
            WindowInfo winInfo = m_platform->getWindowPlacementInfo(clickWindowId);
            int relX = clickX - winInfo.x;
            int relY = clickY - winInfo.y;
            m_platform->simulateClickToWindow(clickWindowId, relX, relY, clickButton, m_action);
        } else if (m_clickMethod == ClickMethod::NoInterference) {
            m_platform->simulateClickNoInterference(clickX, clickY, clickButton, m_action, ClickMethod::NoInterference);
        } else {
            m_platform->simulateClick(clickX, clickY, clickButton, m_action);
        }
        m_totalClicks++;
        m_currentCount++;
        emit clickPerformed(clickX, clickY, clickWindowId);
    }

    // v2.2: Use QTimer::singleShot for non-blocking next click
    // This allows hotkeys to be processed during wait period
    QTimer::singleShot(nextInterval, this, [this]() {
        if (m_running && !m_paused) {
            performClick();
        }
    });
}

void ClickEngine::performWorkflowClick()
{
    if (!m_running || m_paused || m_workflowSequenceIndex < 0 || m_workflowSequenceIndex >= m_workflow.sequences.size()) return;
    if (m_clickCount >= 0 && m_currentCount >= m_clickCount) { stopWorkflowWithReason(QStringLiteral("达到全局点击上限")); return; }
    const WorkflowSequence& sequence = m_workflow.sequences[m_workflowSequenceIndex];
    if (sequence.points.isEmpty()) { stopWorkflowWithReason(QStringLiteral("序列“%1”没有点击点").arg(sequence.name)); return; }
    const SequencePoint& pt = sequence.points[m_workflowPointIndex];
    int x = pt.screenX, y = pt.screenY;
    uintptr_t window = pt.windowId;
    if (pointHasWindowBinding(pt)) {
        window = m_platform->findWindowByLocator(pt.windowTitle, pt.processName, pt.windowClass);
        if (!window) {
            const QString label = QString::fromStdWString(pt.windowTitle).isEmpty()
                ? QString::fromStdWString(pt.windowClass) : QString::fromStdWString(pt.windowTitle);
            stopWorkflowWithReason(QStringLiteral("序列点绑定窗口无法重新定位：%1").arg(label));
            return;
        }
        const WindowInfo info = m_platform->isWindowMinimized(window) ? m_platform->getWindowPlacementInfo(window) : m_platform->getWindowInfo(window);
        if (!info.id) { stopWorkflowWithReason(QStringLiteral("序列点绑定窗口不可用")); return; }
        const ResolvedPointGeometry resolved = resolveSequencePoint(pt, &info);
        x = resolved.globalX;
        y = resolved.globalY;
        m_workflowWindowId = window;
        if (m_clickMethod != ClickMethod::NoInterference
            && (m_platform->isWindowMinimized(window) || m_platform->getForegroundWindowId() != window)) {
            stopWorkflowWithReason(QStringLiteral("序列点绑定窗口不可见、最小化或不在前台：%1").arg(QString::fromStdWString(pt.windowTitle)));
            return;
        }
    }
    if (m_antiDetect) { const QPoint offset = getRandomOffset(); x += offset.x(); y += offset.y(); }
    if (m_clickMethod == ClickMethod::NoInterference && window != 0) {
        const WindowInfo info = m_platform->isWindowMinimized(window) ? m_platform->getWindowPlacementInfo(window) : m_platform->getWindowInfo(window);
        if (!info.id) { stopWorkflowWithReason(QStringLiteral("序列点绑定窗口不可用")); return; }
        m_platform->simulateClickToWindow(window, x - info.x, y - info.y, toMouseButton(pt.button), m_action);
    } else {
        m_platform->simulateClick(x, y, toMouseButton(pt.button), m_action);
    }
    ++m_totalClicks; ++m_currentCount; m_currentTargetWindowId = window; emit clickPerformed(x, y, window);
    ++m_workflowPointClickIndex;
    const int pointClickCount = qMax(1, pt.clickCount);
    if (m_workflowPointClickIndex < pointClickCount) {
        QTimer::singleShot(qMax(1, pt.clickIntervalMs), this, &ClickEngine::performWorkflowClick);
        return;
    }
    m_workflowPointClickIndex = 0;
    ++m_workflowPointIndex;
    const int delay = pt.intervalMs > 0 ? pt.intervalMs : getRandomInterval();
    if (m_workflowPointIndex < sequence.points.size()) { QTimer::singleShot(delay, this, &ClickEngine::performWorkflowClick); return; }
    m_workflowPointIndex = 0; ++m_workflowCompletedLoops;
    emit workflowSequenceChanged(sequence.name, m_workflowSequenceIndex, m_workflowCompletedLoops);
    QTimer::singleShot(delay, this, &ClickEngine::checkWorkflowTransition);
}

void ClickEngine::checkWorkflowTransition()
{
    if (!m_running || m_paused) return;
    WorkflowSequence& sequence = m_workflow.sequences[m_workflowSequenceIndex];
    const bool exhausted = sequence.loopCount != -1 && m_workflowCompletedLoops >= sequence.loopCount;
    bool checked = false;
    if (sequence.transition.enabled && sequence.transition.type == VisualConditionType::LoopEnd) {
        if (exhausted) {
            const int target = sequence.transition.targetSequenceIndex;
            if (target > m_workflowSequenceIndex && target < m_workflow.sequences.size()) {
                m_workflowSequenceIndex = target; m_workflowPointIndex = 0; m_workflowPointClickIndex = 0; m_workflowCompletedLoops = 0; m_workflowConsecutiveHits = 0;
                emit workflowSequenceChanged(m_workflow.sequences[target].name, target, 0);
                QTimer::singleShot(0, this, &ClickEngine::performWorkflowClick); return;
            }
            stopWorkflowWithReason(QStringLiteral("循环结束跳转目标无效")); return;
        }
    } else if (sequence.transition.enabled) {
        // Every completed sequence round performs exactly one visual check.
        checked = true;
        uintptr_t conditionWindow = 0;
        for (const SequencePoint& point : sequence.points) {
            if (!point.hasWindowBinding && point.windowId == 0 && point.windowTitle.empty() && point.processName.empty() && point.windowClass.empty()) continue;
            conditionWindow = m_platform->findWindowByLocator(point.windowTitle, point.processName, point.windowClass);
            if (conditionWindow) break;
        }
        if (!conditionWindow) conditionWindow = m_workflowWindowId;
        if (!conditionWindow) {
            conditionWindow = m_platform->findWindowByLocator(m_workflow.targetWindow.title.toStdWString(), m_workflow.targetWindow.processName.toStdWString(), m_workflow.targetWindow.className.toStdWString());
        }
        if (!conditionWindow || m_platform->isWindowMinimized(conditionWindow) || m_platform->getForegroundWindowId() != conditionWindow) {
            stopWorkflowWithReason(QStringLiteral("视觉条件绑定窗口不可见、最小化或无法重新定位")); return;
        }
        m_workflowWindowId = conditionWindow;
        const QImage image = m_platform->captureWindow(conditionWindow);
        if (image.isNull()) { m_workflowConsecutiveHits = 0; stopWorkflowWithReason(QStringLiteral("无法获取目标窗口画面")); return; }
        QString detail; const bool matched = VisualDetector::matches(image, sequence.transition, &detail);
        m_workflowConsecutiveHits = matched ? 1 : 0;
        emit visualConditionChecked(detail, matched, m_workflowConsecutiveHits);
        if (matched) {
            const int target = sequence.transition.targetSequenceIndex;
            if (target > m_workflowSequenceIndex && target < m_workflow.sequences.size()) {
                m_workflowSequenceIndex = target; m_workflowPointIndex = 0; m_workflowPointClickIndex = 0; m_workflowCompletedLoops = 0; m_workflowConsecutiveHits = 0;
                emit workflowSequenceChanged(m_workflow.sequences[target].name, target, 0);
                QTimer::singleShot(0, this, &ClickEngine::performWorkflowClick); return;
            }
            stopWorkflowWithReason(QStringLiteral("跳转目标无效")); return;
        }
    }
    if (exhausted) {
        const QString reason = checked ? QStringLiteral("序列已完成，条件未命中") : QStringLiteral("序列已完成");
        stopWorkflowWithReason(reason);
        emit workflowCompleted(reason);
        return;
    }
    QTimer::singleShot(0, this, &ClickEngine::performWorkflowClick);
}

int ClickEngine::getRandomInterval()
{
    if (m_jitterRange == 0) {
        return m_intervalBase;
    }
    // Generate random jitter within +/- jitterRange
    int jitter = QRandomGenerator::global()->bounded(-m_jitterRange, m_jitterRange + 1);
    return qMax(1, m_intervalBase + jitter);  // Ensure at least 1ms
}

QPoint ClickEngine::getRandomOffset()
{
    // Random offset within +/- 5 pixels
    int offsetX = QRandomGenerator::global()->bounded(-5, 6);
    int offsetY = QRandomGenerator::global()->bounded(-5, 6);
    return QPoint(offsetX, offsetY);
}

SequencePoint ClickEngine::getNextSequencePoint()
{
    if (m_sequencePoints.isEmpty()) {
        return SequencePoint();
    }

    SequencePoint pt = m_sequencePoints[m_sequenceIndex];
    m_sequenceIndex = (m_sequenceIndex + 1) % m_sequencePoints.size();
    return pt;
}
