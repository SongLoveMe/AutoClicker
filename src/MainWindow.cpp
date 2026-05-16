#include "MainWindow.h"
#include "core/WindowsPlatformAdapter.h"
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
#include <QDebug>
#include <QRegularExpression>
#include <QDialog>
#include <QMenu>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_startBtn(nullptr)
    , m_stopBtn(nullptr)
    , m_pauseBtn(nullptr)
    , m_statusLabel(nullptr)
    , m_clickCountLabel(nullptr)
    , m_buttonCombo(nullptr)
    , m_clickTypeCombo(nullptr)
    , m_intervalSpin(nullptr)
    , m_randomizeCheck(nullptr)
    , m_jitterRangeSpin(nullptr)
    , m_countSpin(nullptr)
    , m_positionLabel(nullptr)
    , m_manualXSpin(nullptr)
    , m_manualYSpin(nullptr)
    , m_addManualBtn(nullptr)
    , m_recordBtn(nullptr)
    , m_recordStatusLabel(nullptr)
    , m_isRecording(false)
    , m_deleteSeqBtn(nullptr)
    , m_moveUpBtn(nullptr)
    , m_moveDownBtn(nullptr)
    , m_clearSeqBtn(nullptr)
    , m_sequenceList(nullptr)
    , m_targetX(0)
    , m_targetY(0)
    , m_currentWindowLabel(nullptr)
    , m_targetWindowCombo(nullptr)
    , m_refreshWindowsBtn(nullptr)
    , m_elementTextEdit(nullptr)
    , m_findElementBtn(nullptr)
    , m_elementInfoLabel(nullptr)
    , m_targetWindowId(0)
    , m_hasTargetElement(false)
    , m_windowUpdateTimer(nullptr)
    , m_platformAdapter(std::make_shared<WindowsPlatformAdapter>())
    , m_clickEngine(std::make_shared<ClickEngine>(m_platformAdapter))
{
    setupUI();
    // Delay hotkey registration until window is fully shown
    // This ensures the window handle is properly realized for RegisterHotKey
    QTimer::singleShot(100, this, &MainWindow::setupHotkeys);
    setupClickEngine();
    setupWindowUpdateTimer();

    // Timer for updating mouse position
    QTimer* mouseTimer = new QTimer(this);
    connect(mouseTimer, &QTimer::timeout, this, &MainWindow::updateMousePosition);
    mouseTimer->start(100);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupHotkeys()
{
#ifdef Q_OS_WIN
    m_platformAdapter->registerHotkey(1, Qt::Key_F6, 0, [this]() {
        if (m_clickEngine->isRunning()) {
            onStopClicked();
        } else {
            onStartClicked();
        }
    });

    m_platformAdapter->registerHotkey(2, Qt::Key_F7, 0, [this]() {
        onPauseClicked();
    });

    qDebug() << "热键已注册: F6 (开始/停止), F7 (暂停)";
#endif
}

void MainWindow::setupClickEngine()
{
    connect(m_clickEngine.get(), &ClickEngine::clickPerformed, this, &MainWindow::onClickPerformed);
    connect(m_clickEngine.get(), &ClickEngine::finished, this, &MainWindow::onEngineFinished);
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        int hotkeyId = static_cast<int>(msg->wParam);
        if (m_platformAdapter->handleHotkeyEvent(hotkeyId)) {
            return true;
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::setupUI()
{
    // 设置窗口标题
    setWindowTitle("通用连点器 v2.1");

    setupToolbar();
    setupStatusBar();

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setStyleSheet("background-color: #f5f5f5;");
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // 标题区域
    QLabel* titleLabel = new QLabel("🎯 通用连点器", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #333; padding: 10px;");
    mainLayout->addWidget(titleLabel);

    // 内容区域
    QHBoxLayout* contentLayout = new QHBoxLayout();

    // 左侧：配置
    QVBoxLayout* leftLayout = new QVBoxLayout();
    leftLayout->addWidget(createConfigGroupBox());

    // 状态显示
    m_statusLabel = new QLabel("就绪", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("font-size: 16px; padding: 15px; background-color: #e0e0e0; border-radius: 8px; color: #555;");
    leftLayout->addWidget(m_statusLabel);

    m_clickCountLabel = new QLabel("点击次数: 0", this);
    m_clickCountLabel->setAlignment(Qt::AlignCenter);
    m_clickCountLabel->setStyleSheet("font-size: 14px; color: #666;");
    leftLayout->addWidget(m_clickCountLabel);

    leftLayout->addStretch();

    // 右侧：坐标和目标窗口
    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->addWidget(createTargetWindowGroupBox());
    rightLayout->addWidget(createPositionGroupBox());
    rightLayout->addStretch();

    contentLayout->addLayout(leftLayout);
    contentLayout->addLayout(rightLayout);

    mainLayout->addLayout(contentLayout);
    setCentralWidget(centralWidget);

    // Window stay on top - ALWAYS enabled, no UI control needed
    setWindowStayOnTop(true);
}

void MainWindow::setupToolbar()
{
    QToolBar* toolbar = addToolBar("主工具栏");
    toolbar->setMovable(false);
    toolbar->setStyleSheet("QToolBar { background: #fff; border-bottom: 1px solid #ddd; padding: 5px; }");

    m_startBtn = new QPushButton("▶ 开始", this);
    m_startBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 10px 25px; font-size: 14px; border-radius: 6px; border: none; } QPushButton:hover { background-color: #45a049; }");
    m_startBtn->setMinimumWidth(100);

    m_stopBtn = new QPushButton("⏹ 停止", this);
    m_stopBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; padding: 10px 25px; font-size: 14px; border-radius: 6px; border: none; } QPushButton:hover { background-color: #da190b; }");
    m_stopBtn->setMinimumWidth(100);

    m_pauseBtn = new QPushButton("⏸ 暂停", this);
    m_pauseBtn->setStyleSheet("QPushButton { background-color: #FF9800; color: white; padding: 10px 25px; font-size: 14px; border-radius: 6px; border: none; } QPushButton:hover { background-color: #e68900; }");
    m_pauseBtn->setMinimumWidth(100);

    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::onPauseClicked);

    toolbar->addWidget(m_startBtn);
    toolbar->addWidget(new QWidget());  // spacer
    toolbar->addWidget(m_stopBtn);
    toolbar->addWidget(new QWidget());  // spacer
    toolbar->addWidget(m_pauseBtn);
}

void MainWindow::setupStatusBar()
{
    statusBar()->setStyleSheet("QStatusBar { background: #fff; color: #666; }");
    statusBar()->showMessage("就绪 | 热键: F6 开始/停止 | F7 暂停");
}

QGroupBox* MainWindow::createConfigGroupBox()
{
    QGroupBox* group = new QGroupBox("参数配置", this);
    group->setStyleSheet("QGroupBox { font-weight: bold; border: 1px solid #ddd; border-radius: 8px; margin-top: 10px; padding-top: 10px; background: #fff; } QGroupBox::title { subcontrol-origin: margin; left: 10px; color: #333; }");
    QGridLayout* layout = new QGridLayout(group);
    layout->setSpacing(8);

    // 点击按键
    QLabel* buttonLabel = new QLabel("点击按键:", this);
    buttonLabel->setStyleSheet("color: #333;");
    m_buttonCombo = new QComboBox(this);
    m_buttonCombo->addItem("左键", "left");
    m_buttonCombo->addItem("右键", "right");
    m_buttonCombo->addItem("中键", "middle");
    m_buttonCombo->setStyleSheet("QComboBox { padding: 5px; border: 1px solid #ddd; border-radius: 4px; background: #fff; }");

    // 点击方式
    QLabel* clickTypeLabel = new QLabel("点击方式:", this);
    clickTypeLabel->setStyleSheet("color: #333;");
    m_clickTypeCombo = new QComboBox(this);
    m_clickTypeCombo->addItem("单击", "single");
    m_clickTypeCombo->addItem("双击", "double");
    m_clickTypeCombo->addItem("三击", "triple");
    m_clickTypeCombo->addItem("长按", "hold");
    m_clickTypeCombo->setStyleSheet("QComboBox { padding: 5px; border: 1px solid #ddd; border-radius: 4px; background: #fff; }");

    // 点击间隔 + 随机化 (合并选项)
    QLabel* intervalLabel = new QLabel("间隔:", this);
    intervalLabel->setStyleSheet("color: #333;");

    m_intervalSpin = new QSpinBox(this);
    m_intervalSpin->setRange(1, 10000);
    m_intervalSpin->setValue(100);
    m_intervalSpin->setSuffix(" ms");
    m_intervalSpin->setStyleSheet("QSpinBox { padding: 5px; border: 1px solid #ddd; border-radius: 4px; background: #fff; }");

    // 随机化 (合并：间隔扰动 + 位置偏移)
    m_randomizeCheck = new QCheckBox("随机化", this);
    m_randomizeCheck->setStyleSheet("QCheckBox { color: #333; padding: 5px; }");
    m_randomizeCheck->setToolTip("启用后，点击间隔和位置会有轻微随机变化，模拟真实操作");

    m_jitterRangeSpin = new QSpinBox(this);
    m_jitterRangeSpin->setRange(1, 500);
    m_jitterRangeSpin->setValue(10);
    m_jitterRangeSpin->setSuffix(" ms");
    m_jitterRangeSpin->setEnabled(false);
    m_jitterRangeSpin->setStyleSheet("QSpinBox { padding: 5px; border: 1px solid #ddd; border-radius: 4px; background: #fff; }");

    connect(m_randomizeCheck, &QCheckBox::toggled, m_jitterRangeSpin, &QSpinBox::setEnabled);

    QHBoxLayout* intervalLayout = new QHBoxLayout();
    intervalLayout->addWidget(m_intervalSpin);
    intervalLayout->addWidget(m_randomizeCheck);
    intervalLayout->addWidget(new QLabel("±", this));
    intervalLayout->addWidget(m_jitterRangeSpin);

    // 点击次数
    QLabel* countLabel = new QLabel("点击次数:", this);
    countLabel->setStyleSheet("color: #333;");
    m_countSpin = new QSpinBox(this);
    m_countSpin->setRange(-1, 1000000);
    m_countSpin->setValue(0);  // Default to 0 (disabled), prevents accidental activation
    m_countSpin->setSpecialValueText("无限");
    m_countSpin->setStyleSheet("QSpinBox { padding: 5px; border: 1px solid #ddd; border-radius: 4px; background: #fff; }");

    // Layout - simplified (4 rows)
    layout->addWidget(buttonLabel, 0, 0);
    layout->addWidget(m_buttonCombo, 0, 1);
    layout->addWidget(clickTypeLabel, 1, 0);
    layout->addWidget(m_clickTypeCombo, 1, 1);
    layout->addWidget(intervalLabel, 2, 0);
    layout->addLayout(intervalLayout, 2, 1);
    layout->addWidget(countLabel, 3, 0);
    layout->addWidget(m_countSpin, 3, 1);

    return group;
}

QGroupBox* MainWindow::createPositionGroupBox()
{
    QGroupBox* group = new QGroupBox("坐标选取", this);
    group->setStyleSheet("QGroupBox { font-weight: bold; border: 1px solid #ddd; border-radius: 8px; margin-top: 10px; padding-top: 10px; background: #fff; } QGroupBox::title { subcontrol-origin: margin; left: 10px; color: #333; }");
    QVBoxLayout* layout = new QVBoxLayout(group);
    layout->setSpacing(8);

    // 当前坐标显示
    m_positionLabel = new QLabel("当前坐标: (0, 0)", this);
    m_positionLabel->setStyleSheet("font-size: 13px; color: #666; padding: 5px; background: #f9f9f9; border-radius: 4px;");
    layout->addWidget(m_positionLabel);

    // 手动坐标输入
    QHBoxLayout* manualCoordLayout = new QHBoxLayout();
    QLabel* manualCoordLabel = new QLabel("手动输入:", this);
    manualCoordLabel->setStyleSheet("color: #666; font-size: 12px;");
    m_manualXSpin = new QSpinBox(this);
    m_manualXSpin->setRange(0, 9999);
    m_manualXSpin->setValue(0);
    m_manualXSpin->setPrefix("X: ");
    m_manualXSpin->setStyleSheet("QSpinBox { padding: 5px; border: 1px solid #ddd; border-radius: 4px; background: #fff; }");
    m_manualYSpin = new QSpinBox(this);
    m_manualYSpin->setRange(0, 9999);
    m_manualYSpin->setValue(0);
    m_manualYSpin->setPrefix("Y: ");
    m_manualYSpin->setStyleSheet("QSpinBox { padding: 5px; border: 1px solid #ddd; border-radius: 4px; background: #fff; }");
    m_addManualBtn = new QPushButton("添加", this);
    m_addManualBtn->setStyleSheet("QPushButton { padding: 6px 12px; background: #FF9800; color: white; border-radius: 4px; border: none; } QPushButton:hover { background: #F57C00; }");
    connect(m_addManualBtn, &QPushButton::clicked, this, &MainWindow::onAddManualCoordinateClicked);
    manualCoordLayout->addWidget(manualCoordLabel);
    manualCoordLayout->addWidget(m_manualXSpin);
    manualCoordLayout->addWidget(m_manualYSpin);
    manualCoordLayout->addWidget(m_addManualBtn);
    layout->addLayout(manualCoordLayout);

    // 录制模式
    QHBoxLayout* recordLayout = new QHBoxLayout();
    m_recordBtn = new QPushButton("🔴 开始录制", this);
    m_recordBtn->setStyleSheet("QPushButton { padding: 8px; background: #4CAF50; color: white; border-radius: 4px; border: none; } QPushButton:hover { background: #45a049; }");
    m_recordBtn->setCheckable(true);
    connect(m_recordBtn, &QPushButton::toggled, this, &MainWindow::onRecordToggled);
    m_recordStatusLabel = new QLabel("未录制", this);
    m_recordStatusLabel->setStyleSheet("color: #666; font-size: 12px; padding: 5px;");
    recordLayout->addWidget(m_recordBtn);
    recordLayout->addWidget(m_recordStatusLabel);
    layout->addLayout(recordLayout);

    // 序列列表
    m_sequenceList = new QListWidget(this);
    m_sequenceList->setMaximumHeight(150);
    m_sequenceList->setStyleSheet("QListWidget { border: 1px solid #ddd; border-radius: 4px; background: #fafafa; } QListWidget::item { padding: 5px; }");
    m_sequenceList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_sequenceList, &QListWidget::customContextMenuRequested, this, &MainWindow::onSequenceContextMenuRequested);
    connect(m_sequenceList, &QListWidget::itemDoubleClicked, this, &MainWindow::onSequenceItemDoubleClicked);
    layout->addWidget(m_sequenceList);

    // 序列编辑按钮
    QHBoxLayout* seqEditLayout = new QHBoxLayout();
    m_deleteSeqBtn = new QPushButton("删除选中", this);
    m_deleteSeqBtn->setStyleSheet("QPushButton { padding: 6px; background: #f44336; color: white; border-radius: 4px; border: none; } QPushButton:hover { background: #d32f2f; }");
    connect(m_deleteSeqBtn, &QPushButton::clicked, this, &MainWindow::onDeleteSequenceItemClicked);

    m_moveUpBtn = new QPushButton("↑", this);
    m_moveUpBtn->setMaximumWidth(40);
    m_moveUpBtn->setStyleSheet("QPushButton { padding: 6px; background: #2196F3; color: white; border-radius: 4px; border: none; } QPushButton:hover { background: #1976D2; }");
    connect(m_moveUpBtn, &QPushButton::clicked, this, &MainWindow::onMoveSequenceItemUp);

    m_moveDownBtn = new QPushButton("↓", this);
    m_moveDownBtn->setMaximumWidth(40);
    m_moveDownBtn->setStyleSheet("QPushButton { padding: 6px; background: #2196F3; color: white; border-radius: 4px; border: none; } QPushButton:hover { background: #1976D2; }");
    connect(m_moveDownBtn, &QPushButton::clicked, this, &MainWindow::onMoveSequenceItemDown);

    m_clearSeqBtn = new QPushButton("清空全部", this);
    m_clearSeqBtn->setStyleSheet("QPushButton { padding: 6px; background: #9E9E9E; color: white; border-radius: 4px; border: none; } QPushButton:hover { background: #757575; }");
    connect(m_clearSeqBtn, &QPushButton::clicked, this, &MainWindow::onClearSequenceClicked);

    seqEditLayout->addWidget(m_deleteSeqBtn);
    seqEditLayout->addWidget(m_moveUpBtn);
    seqEditLayout->addWidget(m_moveDownBtn);
    seqEditLayout->addWidget(m_clearSeqBtn);
    layout->addLayout(seqEditLayout);

    return group;
}

QGroupBox* MainWindow::createTargetWindowGroupBox()
{
    QGroupBox* group = new QGroupBox("目标窗口", this);
    group->setStyleSheet("QGroupBox { font-weight: bold; border: 1px solid #ddd; border-radius: 8px; margin-top: 10px; padding-top: 10px; background: #fff; } QGroupBox::title { subcontrol-origin: margin; left: 10px; color: #333; }");
    QVBoxLayout* layout = new QVBoxLayout(group);
    layout->setSpacing(8);

    // 当前最上层窗口显示
    m_currentWindowLabel = new QLabel("当前窗口: (自动检测)", this);
    m_currentWindowLabel->setStyleSheet("font-size: 12px; color: #666; padding: 5px; background: #f9f9f9; border-radius: 4px;");
    layout->addWidget(m_currentWindowLabel);

    // 目标窗口选择
    QHBoxLayout* targetWinLayout = new QHBoxLayout();
    QLabel* targetWinLabel = new QLabel("指定窗口:", this);
    targetWinLabel->setStyleSheet("color: #333;");
    m_targetWindowCombo = new QComboBox(this);
    m_targetWindowCombo->addItem("自动检测", 0);
    m_targetWindowCombo->setStyleSheet("QComboBox { padding: 5px; border: 1px solid #ddd; border-radius: 4px; background: #fff; }");
    m_refreshWindowsBtn = new QPushButton("🔄", this);
    m_refreshWindowsBtn->setStyleSheet("QPushButton { padding: 5px 10px; background: #2196F3; color: white; border-radius: 4px; border: none; } QPushButton:hover { background: #1976D2; }");
    connect(m_refreshWindowsBtn, &QPushButton::clicked, this, &MainWindow::onRefreshWindowsClicked);
    connect(m_targetWindowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onTargetWindowSelected);
    targetWinLayout->addWidget(targetWinLabel);
    targetWinLayout->addWidget(m_targetWindowCombo);
    targetWinLayout->addWidget(m_refreshWindowsBtn);
    layout->addLayout(targetWinLayout);

    // 元素定位
    QHBoxLayout* elementLayout = new QHBoxLayout();
    QLabel* elementLabel = new QLabel("元素文本:", this);
    elementLabel->setStyleSheet("color: #333;");
    m_elementTextEdit = new QLineEdit(this);
    m_elementTextEdit->setPlaceholderText("如按钮上的文字");
    m_elementTextEdit->setStyleSheet("QLineEdit { padding: 5px; border: 1px solid #ddd; border-radius: 4px; background: #fff; }");
    m_findElementBtn = new QPushButton("查找", this);
    m_findElementBtn->setStyleSheet("QPushButton { padding: 5px 10px; background: #FF9800; color: white; border-radius: 4px; border: none; } QPushButton:hover { background: #F57C00; }");
    connect(m_findElementBtn, &QPushButton::clicked, this, &MainWindow::onFindElementClicked);
    elementLayout->addWidget(elementLabel);
    elementLayout->addWidget(m_elementTextEdit);
    elementLayout->addWidget(m_findElementBtn);
    layout->addLayout(elementLayout);

    // 元素信息显示
    m_elementInfoLabel = new QLabel("未找到元素", this);
    m_elementInfoLabel->setStyleSheet("font-size: 12px; color: #999; padding: 5px; background: #f9f9f9; border-radius: 4px;");
    layout->addWidget(m_elementInfoLabel);

    return group;
}

ClickConfig MainWindow::getConfig() const
{
    ClickConfig config;
    config.targetX = m_targetX;
    config.targetY = m_targetY;
    config.buttonType = m_buttonCombo->currentData().toString();
    config.clickType = m_clickTypeCombo->currentData().toString();
    // Removed: clickMethod - always NoInterference now
    config.intervalBase = m_intervalSpin->value();
    config.useRandomize = m_randomizeCheck->isChecked();  // Combined randomization
    config.jitterRange = m_jitterRangeSpin->value();
    config.clickCount = m_countSpin->value();

    // Use stored sequence points
    config.sequencePoints = m_sequencePoints;

    // Auto-detect mode: if sequence has points, use Sequence mode; otherwise FixedPosition
    if (config.sequencePoints.size() > 0) {
        config.mode = ClickMode::Sequence;
    } else {
        config.mode = ClickMode::FixedPosition;
    }

    return config;
}

MouseButton MainWindow::getButtonFromConfig() const
{
    QString button = m_buttonCombo->currentData().toString();
    if (button == "left") return MouseButton::Left;
    if (button == "right") return MouseButton::Right;
    return MouseButton::Middle;
}

ClickAction MainWindow::getActionFromConfig() const
{
    QString action = m_clickTypeCombo->currentData().toString();
    if (action == "single") return ClickAction::Single;
    if (action == "double") return ClickAction::Double;
    if (action == "triple") return ClickAction::Triple;
    return ClickAction::Hold;
}

void MainWindow::applyConfigToEngine()
{
    ClickConfig config = getConfig();
    m_clickEngine->setMode(config.mode);
    m_clickEngine->setPosition(config.targetX, config.targetY);
    m_clickEngine->setSequence(config.sequencePoints);
    m_clickEngine->setButton(getButtonFromConfig());
    m_clickEngine->setAction(getActionFromConfig());
    m_clickEngine->setInterval(config.intervalBase, config.useRandomize ? config.jitterRange : 0);
    m_clickEngine->setClickCount(config.clickCount);

    // Combined randomization: useRandomize now controls both interval and position
    m_clickEngine->setAntiDetect(config.useRandomize);

    // ALWAYS use NoInterference method (background click)
    m_clickEngine->setClickMethod(ClickMethod::NoInterference);

    // Set window binding
    m_clickEngine->setTargetWindow(m_targetWindowId);
    if (m_hasTargetElement) {
        m_clickEngine->setTargetElement(m_targetElement);
    }
}

void MainWindow::updateMousePosition()
{
    if (m_platformAdapter) {
        QPoint pos = m_platformAdapter->getMousePosition();
        m_positionLabel->setText(QString("当前坐标: (%1, %2)").arg(pos.x()).arg(pos.y()));
    }
}

void MainWindow::onClearSequenceClicked()
{
    m_sequenceList->clear();
    m_sequencePoints.clear();
    m_recordStatusLabel->setText("未录制");
}

void MainWindow::onStartClicked()
{
    applyConfigToEngine();
    m_clickEngine->start();

    m_statusLabel->setText("▶ 运行中...");
    m_statusLabel->setStyleSheet("font-size: 16px; padding: 15px; background-color: #c8e6c9; border-radius: 8px; color: #2e7d32;");
    m_clickCountLabel->setText("点击次数: 0");
    statusBar()->showMessage("运行中 | 按 F6 停止");
}

void MainWindow::onStopClicked()
{
    m_clickEngine->stop();

    m_statusLabel->setText("⏹ 已停止");
    m_statusLabel->setStyleSheet("font-size: 16px; padding: 15px; background-color: #ffcdd2; border-radius: 8px; color: #c62828;");
    m_clickCountLabel->setText(QString("点击次数: %1").arg(m_clickEngine->getTotalClicks()));
    statusBar()->showMessage("已停止 | 总点击: " + QString::number(m_clickEngine->getTotalClicks()));
}

void MainWindow::onPauseClicked()
{
    if (m_clickEngine->isRunning()) {
        if (m_clickEngine->isPaused()) {
            m_clickEngine->resume();
            m_statusLabel->setText("▶ 运行中...");
            m_statusLabel->setStyleSheet("font-size: 16px; padding: 15px; background-color: #c8e6c9; border-radius: 8px; color: #2e7d32;");
            statusBar()->showMessage("运行中 | 按 F6 停止");
        } else {
            m_clickEngine->pause();
            m_statusLabel->setText("⏸ 已暂停");
            m_statusLabel->setStyleSheet("font-size: 16px; padding: 15px; background-color: #ffe0b2; border-radius: 8px; color: #e65100;");
            statusBar()->showMessage("已暂停 | 按 F7 继续");
        }
    }
}

void MainWindow::onClickPerformed(int x, int y, uintptr_t windowId)
{
    int total = m_clickEngine->getTotalClicks();
    m_clickCountLabel->setText(QString("点击次数: %1").arg(total));

    // Track last clicked window for idle display
    m_lastClickedWindowId = windowId;
}

void MainWindow::onEngineFinished()
{
    m_statusLabel->setText("✅ 已完成");
    m_statusLabel->setStyleSheet("font-size: 16px; padding: 15px; background-color: #bbdefb; border-radius: 8px; color: #1565c0;");
    m_clickCountLabel->setText(QString("点击次数: %1").arg(m_clickEngine->getTotalClicks()));
    statusBar()->showMessage("已完成 | 总点击: " + QString::number(m_clickEngine->getTotalClicks()));
}

// Removed: onStayOnTopToggled - stay on top is now always enabled

void MainWindow::setWindowStayOnTop(bool enabled)
{
    Qt::WindowFlags flags = windowFlags();
    if (enabled) {
        flags |= Qt::WindowStaysOnTopHint;
    } else {
        flags &= ~Qt::WindowStaysOnTopHint;
    }
    setWindowFlags(flags);
    show();  // Required after changing window flags
}

void MainWindow::onAddManualCoordinateClicked()
{
    int x = m_manualXSpin->value();
    int y = m_manualYSpin->value();

    // Create a SequencePoint without window binding
    SequencePoint point;
    point.screenX = x;
    point.screenY = y;
    point.button = RecordedButton::Left;  // Default to left button
    point.windowId = 0;  // No window binding

    m_sequencePoints.append(point);
    updateSequenceListDisplay();
}

void MainWindow::onRecordToggled(bool enabled)
{
    m_isRecording = enabled;

    if (enabled) {
        // Start recording - use mouse hook
        // Note: Mouse hook already filters clicks inside AutoClicker window
        bool success = m_platformAdapter->startMouseHook([this](const RecordedClick& click) {
            onMouseClickRecorded(click);
        });

        if (success) {
            m_recordBtn->setText("⏹ 停止录制");
            m_recordBtn->setStyleSheet("QPushButton { padding: 8px; background: #f44336; color: white; border-radius: 4px; border: none; } QPushButton:hover { background: #d32f2f; }");
            m_recordStatusLabel->setText("录制中...");
            m_recordStatusLabel->setStyleSheet("color: #f44336; font-size: 12px; padding: 5px; font-weight: bold;");
        } else {
            m_recordBtn->setChecked(false);
            m_recordStatusLabel->setText("录制失败");
        }
    } else {
        // Stop recording
        m_platformAdapter->stopMouseHook();
        m_recordBtn->setText("🔴 开始录制");
        m_recordBtn->setStyleSheet("QPushButton { padding: 8px; background: #4CAF50; color: white; border-radius: 4px; border: none; } QPushButton:hover { background: #45a049; }");
        m_recordStatusLabel->setText(QString("已录制 %1 个点").arg(m_sequencePoints.size()));
        m_recordStatusLabel->setStyleSheet("color: #666; font-size: 12px; padding: 5px;");
    }
}

void MainWindow::onMouseClickRecorded(const RecordedClick& click)
{
    if (!m_isRecording) return;

    // Create SequencePoint with full information
    SequencePoint point;
    point.screenX = click.x;
    point.screenY = click.y;
    point.button = click.button;

    // Capture window information
    point.windowId = click.windowId;
    if (point.windowId != 0) {
        WindowInfo winInfo = m_platformAdapter->getWindowInfo(point.windowId);
        point.windowTitle = winInfo.title;
        point.processName = winInfo.processName;

        // Calculate relative coordinates
        point.relX = click.x - winInfo.x;
        point.relY = click.y - winInfo.y;
    }

    // Store the SequencePoint
    m_sequencePoints.append(point);

    // Update display
    updateSequenceListDisplay();

    m_recordStatusLabel->setText(QString("录制中... %1 个点").arg(m_sequencePoints.size()));
}

void MainWindow::setupWindowUpdateTimer()
{
    m_windowUpdateTimer = new QTimer(this);
    connect(m_windowUpdateTimer, &QTimer::timeout, this, &MainWindow::updateCurrentWindow);
    m_windowUpdateTimer->start(1000);  // Update every second
}

void MainWindow::updateCurrentWindow()
{
    if (!m_platformAdapter) return;

    QString displayText;
    bool isMinimized = false;

    if (m_clickEngine->isRunning()) {
        // During running: show the window being clicked
        uintptr_t targetWindow = m_clickEngine->getCurrentTargetWindowId();
        if (targetWindow != 0) {
            std::wstring title = m_platformAdapter->getWindowTitleW(targetWindow);
            displayText = QString::fromWCharArray(title.c_str());

            // Check if window is minimized
            isMinimized = m_platformAdapter->isWindowMinimized(targetWindow);
            if (isMinimized) {
                displayText += " [最小化]";
            }
        } else {
            displayText = "无绑定窗口";
        }
    } else {
        // When idle: show last clicked window (not mouse position)
        if (m_lastClickedWindowId != 0) {
            std::wstring title = m_platformAdapter->getWindowTitleW(m_lastClickedWindowId);
            displayText = QString::fromWCharArray(title.c_str());
        } else {
            displayText = "(等待点击检测)";
        }
    }

    if (displayText.length() > 30) {
        displayText = displayText.left(30) + "...";
    }
    m_currentWindowLabel->setText(QString("当前窗口: %1").arg(displayText));

    // Update style based on minimized state
    if (isMinimized) {
        m_currentWindowLabel->setStyleSheet("font-size: 12px; color: #f44336; padding: 5px; background: #ffebee; border-radius: 4px;");
    } else {
        m_currentWindowLabel->setStyleSheet("font-size: 12px; color: #666; padding: 5px; background: #f9f9f9; border-radius: 4px;");
    }
}

void MainWindow::onRefreshWindowsClicked()
{
    m_targetWindowCombo->clear();
    m_targetWindowCombo->addItem("自动检测", 0);

    auto windows = m_platformAdapter->listWindows();
    for (const auto& win : windows) {
        // Use Unicode title
        QString title = QString::fromWCharArray(win.title.c_str());
        if (title.length() > 40) {
            title = title.left(40) + "...";
        }
        m_targetWindowCombo->addItem(title, static_cast<qulonglong>(win.id));
    }

    m_elementInfoLabel->setText(QString("窗口列表已更新 (%1 个)").arg(windows.size()));
}

void MainWindow::onTargetWindowSelected(int index)
{
    if (index < 0) return;

    qulonglong id = m_targetWindowCombo->currentData().toULongLong();
    m_targetWindowId = static_cast<uintptr_t>(id);
    m_hasTargetElement = false;

    if (m_targetWindowId == 0) {
        m_elementInfoLabel->setText("自动检测模式 - 点击当前窗口");
    } else {
        std::wstring title = m_platformAdapter->getWindowTitleW(m_targetWindowId);
        QString displayTitle = QString::fromWCharArray(title.c_str()).left(20);
        m_elementInfoLabel->setText(QString("绑定窗口: %1").arg(displayTitle));
    }
}

void MainWindow::onFindElementClicked()
{
    if (m_targetWindowId == 0) {
        // Use current foreground window
        m_targetWindowId = m_platformAdapter->getForegroundWindowId();
    }

    std::wstring searchText = m_elementTextEdit->text().toStdWString();
    if (searchText.empty()) {
        m_elementInfoLabel->setText("请输入元素文本");
        m_elementInfoLabel->setStyleSheet("font-size: 12px; color: #f44336; padding: 5px; background: #ffebee; border-radius: 4px;");
        return;
    }

    m_targetElement = m_platformAdapter->findElementByText(m_targetWindowId, searchText);

    if (m_targetElement.windowId != 0) {
        m_hasTargetElement = true;
        QString info = QString("找到: \"%1\" @ (%2, %3) %4x%5")
            .arg(QString::fromWCharArray(m_targetElement.text.c_str()).left(15))
            .arg(m_targetElement.relativeX)
            .arg(m_targetElement.relativeY)
            .arg(m_targetElement.width)
            .arg(m_targetElement.height);
        m_elementInfoLabel->setText(info);
        m_elementInfoLabel->setStyleSheet("font-size: 12px; color: #4CAF50; padding: 5px; background: #e8f5e9; border-radius: 4px;");
    } else {
        m_hasTargetElement = false;
        m_elementInfoLabel->setText(QString("未找到: \"%1\"").arg(m_elementTextEdit->text()));
        m_elementInfoLabel->setStyleSheet("font-size: 12px; color: #f44336; padding: 5px; background: #ffebee; border-radius: 4px;");
    }
}

void MainWindow::onDeleteSequenceItemClicked()
{
    int row = m_sequenceList->currentRow();
    if (row >= 0 && row < m_sequencePoints.size()) {
        m_sequenceList->takeItem(row);
        m_sequencePoints.removeAt(row);
        m_recordStatusLabel->setText(QString("已录制 %1 个点").arg(m_sequencePoints.size()));
    }
}

void MainWindow::onMoveSequenceItemUp()
{
    int row = m_sequenceList->currentRow();
    if (row > 0 && row < m_sequencePoints.size()) {
        QListWidgetItem* item = m_sequenceList->takeItem(row);
        m_sequenceList->insertItem(row - 1, item);
        m_sequenceList->setCurrentRow(row - 1);
        m_sequencePoints.swapItemsAt(row, row - 1);
    }
}

void MainWindow::onMoveSequenceItemDown()
{
    int row = m_sequenceList->currentRow();
    if (row >= 0 && row < m_sequencePoints.size() - 1) {
        QListWidgetItem* item = m_sequenceList->takeItem(row);
        m_sequenceList->insertItem(row + 1, item);
        m_sequenceList->setCurrentRow(row + 1);
        m_sequencePoints.swapItemsAt(row, row + 1);
    }
}

void MainWindow::onSequenceItemDoubleClicked(QListWidgetItem* item)
{
    if (!item) return;

    int row = m_sequenceList->row(item);
    if (row < 0 || row >= m_sequencePoints.size()) return;

    SequencePoint& point = m_sequencePoints[row];

    // Create a simple input dialog inline
    QDialog dialog(this);
    dialog.setWindowTitle("编辑坐标");
    dialog.setModal(true);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(10);

    QHBoxLayout* xLayout = new QHBoxLayout();
    QLabel* xLabel = new QLabel("X:", &dialog);
    QSpinBox* xSpin = new QSpinBox(&dialog);
    xSpin->setRange(0, 9999);
    xSpin->setValue(point.screenX);
    xLayout->addWidget(xLabel);
    xLayout->addWidget(xSpin);

    QHBoxLayout* yLayout = new QHBoxLayout();
    QLabel* yLabel = new QLabel("Y:", &dialog);
    QSpinBox* ySpin = new QSpinBox(&dialog);
    ySpin->setRange(0, 9999);
    ySpin->setValue(point.screenY);
    yLayout->addWidget(yLabel);
    yLayout->addWidget(ySpin);

    layout->addLayout(xLayout);
    layout->addLayout(yLayout);

    QPushButton* okBtn = new QPushButton("确定", &dialog);
    okBtn->setStyleSheet("QPushButton { padding: 8px 20px; background: #4CAF50; color: white; border-radius: 4px; }");
    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(okBtn);

    if (dialog.exec() == QDialog::Accepted) {
        point.screenX = xSpin->value();
        point.screenY = ySpin->value();

        // Update relative coordinates if window binding exists
        if (point.windowId != 0) {
            WindowInfo winInfo = m_platformAdapter->getWindowInfo(point.windowId);
            point.relX = point.screenX - winInfo.x;
            point.relY = point.screenY - winInfo.y;
        }

        // Update display
        updateSequenceListDisplay();
        m_sequenceList->setCurrentRow(row);
    }
}

void MainWindow::onSequenceContextMenuRequested(const QPoint& pos)
{
    QListWidgetItem* item = m_sequenceList->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    menu.setStyleSheet("QMenu { background: #fff; border: 1px solid #ddd; } QMenu::item { padding: 5px 20px; } QMenu::item:selected { background: #e3f2fd; }");

    QAction* editAction = menu.addAction("编辑坐标");
    QAction* deleteAction = menu.addAction("删除");
    menu.addSeparator();
    QAction* moveUpAction = menu.addAction("上移");
    QAction* moveDownAction = menu.addAction("下移");

    connect(editAction, &QAction::triggered, this, [this]() {
        onSequenceItemDoubleClicked(m_sequenceList->currentItem());
    });
    connect(deleteAction, &QAction::triggered, this, &MainWindow::onDeleteSequenceItemClicked);
    connect(moveUpAction, &QAction::triggered, this, &MainWindow::onMoveSequenceItemUp);
    connect(moveDownAction, &QAction::triggered, this, &MainWindow::onMoveSequenceItemDown);

    menu.exec(m_sequenceList->mapToGlobal(pos));
}

void MainWindow::updateSequenceListDisplay()
{
    m_sequenceList->clear();

    for (const SequencePoint& point : m_sequencePoints) {
        // Format button type
        QString buttonIcon;
        switch (point.button) {
            case RecordedButton::Left: buttonIcon = "左"; break;
            case RecordedButton::Right: buttonIcon = "右"; break;
            case RecordedButton::Middle: buttonIcon = "中"; break;
        }

        // Format window label
        QString windowLabel;
        if (point.windowId != 0 && !point.windowTitle.empty()) {
            windowLabel = QString::fromWCharArray(point.windowTitle.c_str()).left(15);
        } else {
            windowLabel = "无窗口";
        }

        QString itemText = QString("[%1] (%2, %3) - %4")
            .arg(buttonIcon)
            .arg(point.screenX)
            .arg(point.screenY)
            .arg(windowLabel);

        m_sequenceList->addItem(itemText);
    }
}

void MainWindow::onWindowLabelHovered(QListWidgetItem* item)
{
    if (!item) return;

    uintptr_t windowId = item->data(Qt::UserRole).toULongLong();
    if (windowId != 0) {
        m_platformAdapter->highlightWindow(windowId, 500);
    }
}