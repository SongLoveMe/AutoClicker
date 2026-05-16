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

void ClickEngine::setInterval(int minMs, int maxMs)
{
    m_intervalMin = minMs;
    m_intervalMax = maxMs;
}

void ClickEngine::setClickCount(int count)
{
    m_clickCount = count;
}

void ClickEngine::setAntiDetect(bool enabled)
{
    m_antiDetect = enabled;
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

    // Determine click position based on mode
    int clickX = m_targetX;
    int clickY = m_targetY;

    switch (m_mode) {
        case ClickMode::FixedPosition:
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
                clickX = pt.x();
                clickY = pt.y();
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

    // Perform the click
    if (m_platform) {
        m_platform->simulateClick(clickX, clickY, m_button, m_action);
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
    if (m_intervalMin == m_intervalMax) {
        return m_intervalMin;
    }
    return QRandomGenerator::global()->bounded(m_intervalMin, m_intervalMax + 1);
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