#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QListWidget>
#include <QLineEdit>
#include <QTimer>
#include <QDateTime>  // v2.2: for recording time intervals
#include <memory>
#include "core/PlatformAdapter.h"
#include "core/ClickEngine.h"

struct ClickConfig {
    ClickMode mode = ClickMode::FixedPosition;
    int targetX = 0;
    int targetY = 0;
    QList<SequencePoint> sequencePoints;  // Enhanced sequence with window binding
    QString buttonType = "left";
    QString clickType = "single";
    int intervalBase = 100;       // Base interval in ms
    int jitterRange = 10;         // Jitter range in ms (+/-)
    bool useRandomize = false;    // Combined: interval jitter + position offset
    int clickCount = -1;
    ClickMethod clickMethod = ClickMethod::NoInterference;  // v2.2: restored for game compatibility
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
    void onAddManualCoordinateClicked();
    void onRecordToggled(bool enabled);
    void onMouseClickRecorded(const RecordedClick& click);
    void onRefreshWindowsClicked();
    void onTargetWindowSelected(int index);
    void onFindElementClicked();
    // Removed: onStayOnTopToggled - stay on top is now always enabled
    void updateMousePosition();
    void updateCurrentWindow();
    void onClickPerformed(int x, int y, uintptr_t windowId);
    void onEngineFinished();

    // Sequence editing slots
    void onClearSequenceClicked();
    void onDeleteSequenceItemClicked();
    void onMoveSequenceItemUp();
    void onMoveSequenceItemDown();
    void onSequenceItemDoubleClicked(QListWidgetItem* item);
    void onSequenceContextMenuRequested(const QPoint& pos);

    // Window label slots
    void onWindowLabelHovered(QListWidgetItem* item);

private:
    void setupUI();
    void setupToolbar();
    void setupStatusBar();
    void setupHotkeys();
    void setupClickEngine();
    void setupWindowUpdateTimer();
    void applyConfigToEngine();
    void setWindowStayOnTop(bool enabled);
    QGroupBox* createConfigGroupBox();
    QGroupBox* createPositionGroupBox();
    QGroupBox* createTargetWindowGroupBox();
    QGroupBox* createWindowLabelsGroupBox();
    MouseButton getButtonFromConfig() const;
    ClickAction getActionFromConfig() const;
    void updateSequenceListDisplay();

    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    QPushButton* m_pauseBtn;
    QLabel* m_statusLabel;
    QLabel* m_clickCountLabel;

    QComboBox* m_buttonCombo;
    QComboBox* m_clickTypeCombo;
    QComboBox* m_clickMethodCombo;  // v2.2: restored for game compatibility
    QSpinBox* m_intervalSpin;           // Base interval
    QCheckBox* m_randomizeCheck;        // Combined: interval jitter + position offset
    QSpinBox* m_jitterRangeSpin;        // Randomize range (+/- ms)
    QSpinBox* m_countSpin;
    // Removed: m_antiDetectCheck - merged into m_randomizeCheck
    // Removed: m_stayOnTopCheck - always enabled now

    QLabel* m_positionLabel;
    QSpinBox* m_manualXSpin;        // Manual X coordinate input
    QSpinBox* m_manualYSpin;        // Manual Y coordinate input
    QPushButton* m_addManualBtn;    // Add manual coordinate button
    QPushButton* m_recordBtn;       // Record mode toggle button
    QLabel* m_recordStatusLabel;    // Recording status label
    bool m_isRecording = false;
    QDateTime m_lastRecordTime;  // v2.2: for calculating interval between recorded points

    // Sequence editing buttons
    QPushButton* m_deleteSeqBtn;    // Delete selected item
    QPushButton* m_moveUpBtn;       // Move item up
    QPushButton* m_moveDownBtn;     // Move item down
    QPushButton* m_clearSeqBtn;     // Clear all
    QListWidget* m_sequenceList;

    // Enhanced sequence storage
    QList<SequencePoint> m_sequencePoints;  // Full sequence data

    int m_targetX;
    int m_targetY;
    bool m_stayOnTop = false;

    // Target window binding
    QLabel* m_currentWindowLabel;
    QComboBox* m_targetWindowCombo;
    QPushButton* m_refreshWindowsBtn;
    QLineEdit* m_elementTextEdit;
    QPushButton* m_findElementBtn;
    QLabel* m_elementInfoLabel;
    uintptr_t m_targetWindowId = 0;
    ElementInfo m_targetElement;
    bool m_hasTargetElement = false;
    QTimer* m_windowUpdateTimer;

    // Window tracking
    uintptr_t m_lastClickedWindowId = 0;  // For click-based window detection

    // Window labels UI
    QListWidget* m_windowLabelsList;
    QPushButton* m_highlightWindowBtn;
    QMap<uintptr_t, QString> m_windowLabels;  // Window ID -> user label map

    std::shared_ptr<PlatformAdapter> m_platformAdapter;
    std::shared_ptr<ClickEngine> m_clickEngine;
};

#endif // MAINWINDOW_H