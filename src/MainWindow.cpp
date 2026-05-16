#include "MainWindow.h"
#include <QToolBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QWidget>
#include <QStatusBar>
#include <QGroupBox>
#include <QTimer>
#include <QApplication>
#include <QMouseEvent>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_startBtn(nullptr)
    , m_stopBtn(nullptr)
    , m_pauseBtn(nullptr)
    , m_statusLabel(nullptr)
    , m_clickCountLabel(nullptr)
    , m_fixedPosRadio(nullptr)
    , m_followCursorRadio(nullptr)
    , m_sequenceRadio(nullptr)
    , m_randomRadio(nullptr)
    , m_buttonCombo(nullptr)
    , m_clickTypeCombo(nullptr)
    , m_intervalMinSpin(nullptr)
    , m_intervalMaxSpin(nullptr)
    , m_countSpin(nullptr)
    , m_antiDetectCheck(nullptr)
    , m_positionLabel(nullptr)
    , m_pickPosBtn(nullptr)
    , m_addSeqBtn(nullptr)
    , m_clearSeqBtn(nullptr)
    , m_sequenceList(nullptr)
    , m_isRunning(false)
    , m_isPaused(false)
    , m_totalClicks(0)
{
    setupUI();

    // Timer for updating mouse position
    QTimer* mouseTimer = new QTimer(this);
    connect(mouseTimer, &QTimer::timeout, this, &MainWindow::updateMousePosition);
    mouseTimer->start(100);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    setupToolbar();
    setupStatusBar();

    QWidget* centralWidget = new QWidget(this);
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);

    // Left panel: Mode selection
    QVBoxLayout* leftLayout = new QVBoxLayout();
    leftLayout->addWidget(createModeGroupBox());
    leftLayout->addStretch();

    // Middle panel: Configuration
    QVBoxLayout* middleLayout = new QVBoxLayout();
    middleLayout->addWidget(createConfigGroupBox());

    // Status indicator
    m_statusLabel = new QLabel("Ready", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("font-size: 16px; padding: 15px; background-color: #f0f0f0; border-radius: 5px;");
    middleLayout->addWidget(m_statusLabel);

    m_clickCountLabel = new QLabel("Clicks: 0", this);
    m_clickCountLabel->setAlignment(Qt::AlignCenter);
    middleLayout->addWidget(m_clickCountLabel);

    middleLayout->addStretch();

    // Right panel: Position
    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->addWidget(createPositionGroupBox());
    rightLayout->addStretch();

    mainLayout->addLayout(leftLayout);
    mainLayout->addLayout(middleLayout);
    mainLayout->addLayout(rightLayout);

    setCentralWidget(centralWidget);
}

void MainWindow::setupToolbar()
{
    QToolBar* toolbar = addToolBar("Main Toolbar");
    toolbar->setMovable(false);

    m_startBtn = new QPushButton("Start", this);
    m_startBtn->setStyleSheet("background-color: #4CAF50; color: white; padding: 8px 20px; font-size: 14px;");
    m_startBtn->setMinimumWidth(80);

    m_stopBtn = new QPushButton("Stop", this);
    m_stopBtn->setStyleSheet("background-color: #f44336; color: white; padding: 8px 20px; font-size: 14px;");
    m_stopBtn->setMinimumWidth(80);

    m_pauseBtn = new QPushButton("Pause", this);
    m_pauseBtn->setStyleSheet("background-color: #FF9800; color: white; padding: 8px 20px; font-size: 14px;");
    m_pauseBtn->setMinimumWidth(80);

    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::onPauseClicked);

    toolbar->addWidget(m_startBtn);
    toolbar->addWidget(m_stopBtn);
    toolbar->addWidget(m_pauseBtn);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("Ready | Hotkey: F6 Start/Stop | F7 Pause");
}

QGroupBox* MainWindow::createModeGroupBox()
{
    QGroupBox* group = new QGroupBox("Click Mode", this);
    QVBoxLayout* layout = new QVBoxLayout(group);

    m_fixedPosRadio = new QRadioButton("Fixed Position", this);
    m_fixedPosRadio->setChecked(true);

    m_followCursorRadio = new QRadioButton("Follow Cursor", this);
    m_sequenceRadio = new QRadioButton("Sequence", this);
    m_randomRadio = new QRadioButton("Random Area", this);

    connect(m_fixedPosRadio, &QRadioButton::toggled, this, &MainWindow::onModeChanged);
    connect(m_followCursorRadio, &QRadioButton::toggled, this, &MainWindow::onModeChanged);
    connect(m_sequenceRadio, &QRadioButton::toggled, this, &MainWindow::onModeChanged);
    connect(m_randomRadio, &QRadioButton::toggled, this, &MainWindow::onModeChanged);

    layout->addWidget(m_fixedPosRadio);
    layout->addWidget(m_followCursorRadio);
    layout->addWidget(m_sequenceRadio);
    layout->addWidget(m_randomRadio);

    return group;
}

QGroupBox* MainWindow::createConfigGroupBox()
{
    QGroupBox* group = new QGroupBox("Configuration", this);
    QGridLayout* layout = new QGridLayout(group);

    // Click button type
    QLabel* buttonLabel = new QLabel("Click Button:", this);
    m_buttonCombo = new QComboBox(this);
    m_buttonCombo->addItem("Left", "left");
    m_buttonCombo->addItem("Right", "right");
    m_buttonCombo->addItem("Middle", "middle");

    // Click type
    QLabel* clickTypeLabel = new QLabel("Click Type:", this);
    m_clickTypeCombo = new QComboBox(this);
    m_clickTypeCombo->addItem("Single", "single");
    m_clickTypeCombo->addItem("Double", "double");
    m_clickTypeCombo->addItem("Triple", "triple");
    m_clickTypeCombo->addItem("Hold", "hold");

    // Interval
    QLabel* intervalLabel = new QLabel("Interval (ms):", this);
    QHBoxLayout* intervalLayout = new QHBoxLayout();
    m_intervalMinSpin = new QSpinBox(this);
    m_intervalMinSpin->setRange(1, 10000);
    m_intervalMinSpin->setValue(100);
    m_intervalMaxSpin = new QSpinBox(this);
    m_intervalMaxSpin->setRange(1, 10000);
    m_intervalMaxSpin->setValue(100);
    intervalLayout->addWidget(m_intervalMinSpin);
    intervalLayout->addWidget(new QLabel("-", this));
    intervalLayout->addWidget(m_intervalMaxSpin);

    // Click count
    QLabel* countLabel = new QLabel("Click Count:", this);
    m_countSpin = new QSpinBox(this);
    m_countSpin->setRange(-1, 1000000);
    m_countSpin->setValue(-1);
    m_countSpin->setSpecialValueText("Infinite");

    // Anti-detection
    m_antiDetectCheck = new QCheckBox("Anti-Detection", this);

    // Layout
    layout->addWidget(buttonLabel, 0, 0);
    layout->addWidget(m_buttonCombo, 0, 1);
    layout->addWidget(clickTypeLabel, 1, 0);
    layout->addWidget(m_clickTypeCombo, 1, 1);
    layout->addWidget(intervalLabel, 2, 0);
    layout->addLayout(intervalLayout, 2, 1);
    layout->addWidget(countLabel, 3, 0);
    layout->addWidget(m_countSpin, 3, 1);
    layout->addWidget(m_antiDetectCheck, 4, 0, 1, 2);

    return group;
}

QGroupBox* MainWindow::createPositionGroupBox()
{
    QGroupBox* group = new QGroupBox("Position", this);
    QVBoxLayout* layout = new QVBoxLayout(group);

    // Current position display
    m_positionLabel = new QLabel("Current: (0, 0)", this);
    m_positionLabel->setStyleSheet("font-size: 12px;");
    layout->addWidget(m_positionLabel);

    // Pick position button
    m_pickPosBtn = new QPushButton("Pick Position", this);
    m_pickPosBtn->setStyleSheet("padding: 10px;");
    connect(m_pickPosBtn, &QPushButton::clicked, this, &MainWindow::onPickPositionClicked);
    layout->addWidget(m_pickPosBtn);

    // Sequence controls
    QHBoxLayout* seqBtnLayout = new QHBoxLayout();
    m_addSeqBtn = new QPushButton("Add to Seq", this);
    m_clearSeqBtn = new QPushButton("Clear Seq", this);
    connect(m_addSeqBtn, &QPushButton::clicked, this, &MainWindow::onAddToSequenceClicked);
    connect(m_clearSeqBtn, &QPushButton::clicked, this, &MainWindow::onClearSequenceClicked);
    seqBtnLayout->addWidget(m_addSeqBtn);
    seqBtnLayout->addWidget(m_clearSeqBtn);
    layout->addLayout(seqBtnLayout);

    // Sequence list
    m_sequenceList = new QListWidget(this);
    m_sequenceList->setMaximumHeight(150);
    layout->addWidget(m_sequenceList);

    return group;
}

ClickConfig MainWindow::getConfig() const
{
    ClickConfig config;

    if (m_fixedPosRadio->isChecked())
        config.mode = ClickMode::FixedPosition;
    else if (m_followCursorRadio->isChecked())
        config.mode = ClickMode::FollowCursor;
    else if (m_sequenceRadio->isChecked())
        config.mode = ClickMode::Sequence;
    else if (m_randomRadio->isChecked())
        config.mode = ClickMode::RandomArea;

    config.buttonType = m_buttonCombo->currentData().toString();
    config.clickType = m_clickTypeCombo->currentData().toString();
    config.intervalMin = m_intervalMinSpin->value();
    config.intervalMax = m_intervalMaxSpin->value();
    config.clickCount = m_countSpin->value();
    config.antiDetect = m_antiDetectCheck->isChecked();

    return config;
}

void MainWindow::onModeChanged()
{
    bool isSequence = m_sequenceRadio->isChecked();
    m_addSeqBtn->setEnabled(isSequence);
    m_clearSeqBtn->setEnabled(isSequence);
    m_sequenceList->setEnabled(isSequence);
}

void MainWindow::updateMousePosition()
{
    QPoint pos = QCursor::pos();
    m_positionLabel->setText(QString("Current: (%1, %2)").arg(pos.x()).arg(pos.y()));
}

void MainWindow::onPickPositionClicked()
{
    // Simple implementation: just capture current position
    QPoint pos = QCursor::pos();
    m_positionLabel->setText(QString("Target: (%1, %2)").arg(pos.x()).arg(pos.y()));
}

void MainWindow::onAddToSequenceClicked()
{
    QPoint pos = QCursor::pos();
    QString itemText = QString("(%1, %2)").arg(pos.x()).arg(pos.y());
    m_sequenceList->addItem(itemText);
}

void MainWindow::onClearSequenceClicked()
{
    m_sequenceList->clear();
}

void MainWindow::onStartClicked()
{
    m_isRunning = true;
    m_isPaused = false;
    m_totalClicks = 0;
    m_statusLabel->setText("Running...");
    m_statusLabel->setStyleSheet("font-size: 16px; padding: 15px; background-color: #c8e6c9; border-radius: 5px;");
    m_clickCountLabel->setText("Clicks: 0");
    statusBar()->showMessage("Running | Press F6 to stop");
}

void MainWindow::onStopClicked()
{
    m_isRunning = false;
    m_isPaused = false;
    m_statusLabel->setText("Stopped");
    m_statusLabel->setStyleSheet("font-size: 16px; padding: 15px; background-color: #ffcdd2; border-radius: 5px;");
    m_clickCountLabel->setText(QString("Clicks: %1").arg(m_totalClicks));
    statusBar()->showMessage("Stopped | Total clicks: " + QString::number(m_totalClicks));
}

void MainWindow::onPauseClicked()
{
    if (m_isRunning) {
        m_isPaused = !m_isPaused;
        if (m_isPaused) {
            m_statusLabel->setText("Paused");
            m_statusLabel->setStyleSheet("font-size: 16px; padding: 15px; background-color: #ffe0b2; border-radius: 5px;");
            statusBar()->showMessage("Paused | Press F7 to resume");
        } else {
            m_statusLabel->setText("Running...");
            m_statusLabel->setStyleSheet("font-size: 16px; padding: 15px; background-color: #c8e6c9; border-radius: 5px;");
            statusBar()->showMessage("Running | Press F6 to stop");
        }
    }
}