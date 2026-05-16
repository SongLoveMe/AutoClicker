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
    int intervalMin = 100;
    int intervalMax = 100;
    int clickCount = -1;  // -1 means infinite
    bool antiDetect = false;
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
    void onModeChanged();
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

    // Mode selection
    QRadioButton* m_fixedPosRadio;
    QRadioButton* m_followCursorRadio;
    QRadioButton* m_sequenceRadio;
    QRadioButton* m_randomRadio;

    // Config controls
    QComboBox* m_buttonCombo;
    QComboBox* m_clickTypeCombo;
    QSpinBox* m_intervalMinSpin;
    QSpinBox* m_intervalMaxSpin;
    QSpinBox* m_countSpin;
    QCheckBox* m_antiDetectCheck;

    // Position controls
    QLabel* m_positionLabel;
    QPushButton* m_pickPosBtn;
    QPushButton* m_addSeqBtn;
    QPushButton* m_clearSeqBtn;
    QListWidget* m_sequenceList;
    int m_targetX;
    int m_targetY;

    std::shared_ptr<PlatformAdapter> m_platformAdapter;
    std::shared_ptr<ClickEngine> m_clickEngine;
};

#endif // MAINWINDOW_H