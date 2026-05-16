#ifndef CLICKENGINE_H
#define CLICKENGINE_H

#include <QObject>
#include <QTimer>
#include <memory>
#include "PlatformAdapter.h"

class ClickEngine : public QObject
{
    Q_OBJECT

public:
    explicit ClickEngine(std::shared_ptr<PlatformAdapter> platform, QObject* parent = nullptr);
    ~ClickEngine();

    void setMode(ClickMode mode);
    void setPosition(int x, int y);
    void setSequence(const QList<SequencePoint>& points);  // Enhanced sequence
    void setButton(MouseButton button);
    void setAction(ClickAction action);
    void setInterval(int baseMs, int jitterMs);  // base interval + optional jitter range
    void setClickCount(int count);  // -1 for infinite
    void setAntiDetect(bool enabled);
    void setClickMethod(ClickMethod method);
    void setTargetWindow(uintptr_t windowId);
    void setTargetElement(const ElementInfo& element);

    void start();
    void stop();
    void pause();
    void resume();

    bool isRunning() const;
    bool isPaused() const;
    int getTotalClicks() const;
    uintptr_t getCurrentTargetWindowId() const;  // For UI display during running

signals:
    void clickPerformed(int x, int y, uintptr_t windowId);  // Include window ID
    void finished();
    void errorOccurred(const QString& error);

private slots:
    void performClick();

private:
    int getRandomInterval();
    QPoint getRandomOffset();
    SequencePoint getNextSequencePoint();  // Return full SequencePoint

    std::shared_ptr<PlatformAdapter> m_platform;
    QTimer* m_timer;

    ClickMode m_mode = ClickMode::FixedPosition;
    int m_targetX = 0;
    int m_targetY = 0;
    QList<SequencePoint> m_sequencePoints;  // Enhanced sequence with window binding
    int m_sequenceIndex = 0;
    MouseButton m_button = MouseButton::Left;
    ClickAction m_action = ClickAction::Single;
    ClickMethod m_clickMethod = ClickMethod::NoInterference;  // Default to no interference
    int m_intervalBase = 100;     // Base interval in ms
    int m_jitterRange = 0;        // Jitter range (+/- ms), 0 means no jitter
    int m_clickCount = -1;
    bool m_antiDetect = false;

    // Window binding
    uintptr_t m_targetWindowId = 0;
    ElementInfo m_targetElement;
    bool m_hasTargetElement = false;

    // Current target window for UI display
    uintptr_t m_currentTargetWindowId = 0;

    bool m_running = false;
    bool m_paused = false;
    int m_totalClicks = 0;
    int m_currentCount = 0;
};

#endif // CLICKENGINE_H