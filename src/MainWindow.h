#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QListWidget>
#include <memory>
#include "core/PlatformAdapter.h"
#include "core/ClickEngine.h"

struct ClickConfig {
    ClickMode mode = ClickMode::FixedPosition;
    int targetX = 0;
    int targetY = 0;
    QList<QPoint> sequencePoints;
    QString buttonType = "left";
    QString clickType = "single";
    int intervalBase = 100;       // Base interval in ms
    int jitterRange = 10;         // Jitter range in ms (+/-)
    bool useRandomJitter = false; // Enable random jitter
    int clickCount = -1;
    bool antiDetect = false;
    ClickMethod clickMethod = ClickMethod::NoInterference;  // Default to no interference
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    ClickConfig getConfig() const;

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private slots:
    void onStartClicked();
    void onStopClicked();
    void onPauseClicked();
    void onPickPositionClicked();
    void onAddToSequenceClicked();
    void onClearSequenceClicked();
    void onAddManualCoordinateClicked();
    void onModeChanged();
    void onStayOnTopToggled(bool enabled);
    void updateMousePosition();
    void onClickPerformed(int x, int y);
    void onEngineFinished();

private:
    void setupUI();
    void setupToolbar();
    void setupStatusBar();
    void setupHotkeys();
    void setupClickEngine();
    void applyConfigToEngine();
    void setWindowStayOnTop(bool enabled);
    QGroupBox* createModeGroupBox();
    QGroupBox* createConfigGroupBox();
    QGroupBox* createPositionGroupBox();
    MouseButton getButtonFromConfig() const;
    ClickAction getActionFromConfig() const;
    ClickMode getModeFromConfig() const;

    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    QPushButton* m_pauseBtn;
    QLabel* m_statusLabel;
    QLabel* m_clickCountLabel;

    QRadioButton* m_fixedPosRadio;
    QRadioButton* m_followCursorRadio;
    QRadioButton* m_sequenceRadio;
    QRadioButton* m_randomRadio;

    QComboBox* m_buttonCombo;
    QComboBox* m_clickTypeCombo;
    QComboBox* m_clickMethodCombo;     // Click method (SendMessage/SendInput)
    QSpinBox* m_intervalSpin;           // Base interval
    QCheckBox* m_randomJitterCheck;     // Enable random jitter
    QSpinBox* m_jitterRangeSpin;        // Jitter range (+/- ms)
    QSpinBox* m_countSpin;
    QCheckBox* m_antiDetectCheck;
    QCheckBox* m_stayOnTopCheck;        // Window stay on top

    QLabel* m_positionLabel;
    QPushButton* m_pickPosBtn;
    QSpinBox* m_manualXSpin;        // Manual X coordinate input
    QSpinBox* m_manualYSpin;        // Manual Y coordinate input
    QPushButton* m_addManualBtn;    // Add manual coordinate button
    QPushButton* m_addSeqBtn;
    QPushButton* m_clearSeqBtn;
    QListWidget* m_sequenceList;
    int m_targetX;
    int m_targetY;
    bool m_stayOnTop = false;

    std::shared_ptr<PlatformAdapter> m_platformAdapter;
    std::shared_ptr<ClickEngine> m_clickEngine;
};

#endif // MAINWINDOW_H