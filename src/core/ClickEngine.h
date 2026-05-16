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
    void setSequence(const QList<QPoint>& points);
    void setButton(MouseButton button);
    void setAction(ClickAction action);
    void setInterval(int minMs, int maxMs);
    void setClickCount(int count);  // -1 for infinite
    void setAntiDetect(bool enabled);

    void start();
    void stop();
    void pause();
    void resume();

    bool isRunning() const;
    bool isPaused() const;
    int getTotalClicks() const;

signals:
    void clickPerformed(int x, int y);
    void finished();
    void errorOccurred(const QString& error);

private slots:
    void performClick();

private:
    int getRandomInterval();
    QPoint getRandomOffset();
    QPoint getNextSequencePoint();

    std::shared_ptr<PlatformAdapter> m_platform;
    QTimer* m_timer;

    ClickMode m_mode = ClickMode::FixedPosition;
    int m_targetX = 0;
    int m_targetY = 0;
    QList<QPoint> m_sequencePoints;
    int m_sequenceIndex = 0;
    MouseButton m_button = MouseButton::Left;
    ClickAction m_action = ClickAction::Single;
    int m_intervalMin = 100;
    int m_intervalMax = 100;
    int m_clickCount = -1;
    bool m_antiDetect = false;

    bool m_running = false;
    bool m_paused = false;
    int m_totalClicks = 0;
    int m_currentCount = 0;
};

#endif // CLICKENGINE_H