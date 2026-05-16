#include "ClickEngine.h"
#include <QRandomGenerator>
#include <QDebug>

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

void ClickEngine::setSequence(const QList<QPoint>& points)
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

void ClickEngine::start()
{
    if (m_running) {
        return;
    }

    m_running = true;
    m_paused = false;
    m_totalClicks = 0;
    m_currentCount = 0;
    m_sequenceIndex = 0;

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

int ClickEngine::getTotalClicks() const
{
    return m_totalClicks;
}

void ClickEngine::performClick()
{
    // Stop timer to calculate next interval
    m_timer->stop();

    // Check if we've reached the click count limit
    if (m_clickCount > 0 && m_currentCount >= m_clickCount) {
        stop();
        return;
    }

    // Handle window binding - activate target window if specified
    if (m_platform && m_targetWindowId != 0) {
        m_platform->setForegroundWindow(m_targetWindowId);
    }

    // Determine click position based on mode and window binding
    int clickX = m_targetX;
    int clickY = m_targetY;

    // If we have a target element, use its position
    if (m_hasTargetElement) {
        if (m_targetWindowId != 0) {
            QPoint windowPos = m_platform->getWindowPosition(m_targetWindowId);
            clickX = windowPos.x() + m_targetElement.relativeX + m_targetElement.width / 2;
            clickY = windowPos.y() + m_targetElement.relativeY + m_targetElement.height / 2;
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
                    QPoint pt = getNextSequencePoint();
                    // Apply window binding to sequence points
                    if (m_targetWindowId != 0) {
                        QPoint windowPos = m_platform->getWindowPosition(m_targetWindowId);
                        clickX = windowPos.x() + pt.x();
                        clickY = windowPos.y() + pt.y();
                    } else {
                        clickX = pt.x();
                        clickY = pt.y();
                    }
                    if (m_antiDetect) {
                        QPoint offset = getRandomOffset();
                        clickX += offset.x();
                        clickY += offset.y();
                    }
                }
                break;

            case ClickMode::RandomArea:
                // TODO: Implement random area mode
                qWarning() << "Random area mode not implemented yet";
                break;

            case ClickMode::Drag:
                // TODO: Implement drag mode
                qWarning() << "Drag mode not implemented yet";
                break;
        }
    }

    // Perform the click
    if (m_platform) {
        // If we have a target element and using no-interference, click directly on element
        if (m_hasTargetElement && m_clickMethod == ClickMethod::NoInterference) {
            m_platform->clickElement(m_targetElement.windowId, m_button, m_action);
        } else if (m_clickMethod == ClickMethod::NoInterference) {
            // Use no-interference click method
            m_platform->simulateClickNoInterference(clickX, clickY, m_button, m_action, ClickMethod::NoInterference);
        } else {
            // Use traditional SendInput method
            m_platform->simulateClick(clickX, clickY, m_button, m_action);
        }
        m_totalClicks++;
        m_currentCount++;
        emit clickPerformed(clickX, clickY);
    }

    // Calculate next interval and restart timer
    int nextInterval = getRandomInterval();
    m_timer->start(nextInterval);
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

QPoint ClickEngine::getNextSequencePoint()
{
    if (m_sequencePoints.isEmpty()) {
        return QPoint(0, 0);
    }

    QPoint pt = m_sequencePoints[m_sequenceIndex];
    m_sequenceIndex = (m_sequenceIndex + 1) % m_sequencePoints.size();
    return pt;
}