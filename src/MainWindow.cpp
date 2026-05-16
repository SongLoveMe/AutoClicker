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
    , m_fixedPosRadio(nullptr)
    , m_followCursorRadio(nullptr)
    , m_sequenceRadio(nullptr)
    , m_randomRadio(nullptr)
    , m_buttonCombo(nullptr)
    , m_clickTypeCombo(nullptr)
    , m_clickMethodCombo(nullptr)
    , m_intervalSpin(nullptr)
    , m_randomJitterCheck(nullptr)
    , m_jitterRangeSpin(nullptr)
    , m_countSpin(nullptr)
    , m_antiDetectCheck(nullptr)
    , m_stayOnTopCheck(nullptr)
    , m_positionLabel(nullptr)
    , m_pickPosBtn(nullptr)
    , m_manualXSpin(nullptr)
    , m_manualYSpin(nullptr)
    , m_addManualBtn(nullptr)
    , m_addSeqBtn(nullptr)
    , m_clearSeqBtn(nullptr)
    , m_sequenceList(nullptr)
    , m_targetX(0)
    , m_targetY(0)
    , m_stayOnTop(false)
    , m_platformAdapter(std::make_shared<WindowsPlatformAdapter>())
    , m_clickEngine(std::make_shared<ClickEngine>(m_platformAdapter))
{
    setupUI();
    setupHotkeys();
    setupClickEngine();

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
    setWindowTitle("通用连点器 v1.0");

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

    // 左侧：模式选择
    QVBoxLayout* leftLayout = new QVBoxLayout();
    leftLayout->addWidget(createModeGroupBox());
    leftLayout->addStretch();

    // 中间：配置
    QVBoxLayout* middleLayout = new QVBoxLayout();
    middleLayout->addWidget(createConfigGroupBox());

    // 状态显示
    m_statusLabel = new QLabel("就绪", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("font-size: 16px; padding: 15px; background-color: #e0e0e0; border-radius: 8px; color: #555;");
    middleLayout->addWidget(m_statusLabel);

    m_clickCountLabel = new QLabel("点击次数: 0", this);
    m_clickCountLabel->setAlignment(Qt::AlignCenter);
    m_clickCountLabel->setStyleSheet("font-size: 14px; color: #666;");
    middleLayout->addWidget(m_clickCountLabel);

    middleLayout->addStretch();

    // 右侧：坐标
    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->addWidget(createPositionGroupBox());
    rightLayout->addStretch();

    contentLayout->addLayout(leftLayout);
    contentLayout->addLayout(middleLayout);
    contentLayout->addLayout(rightLayout);

    mainLayout->addLayout(contentLayout);
    setCentralWidget(centralWidget);
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

QGroupBox* MainWindow::createModeGroupBox()
{
    QGroupBox* group = new QGroupBox("点击模式", this);
    group->setStyleSheet("QGroupBox { font-weight: bold; border: 1px solid #ddd; border-radius: 8px; margin-top: 10px; padding-top: 10px; background: #fff; } QGroupBox::title { subcontrol-origin: margin; left: 10px; color: #333; }");
    QVBoxLayout* layout = new QVBoxLayout(group);
    layout->setSpacing(8);

    m_fixedPosRadio = new QRadioButton("📍 固定位置", this);
    m_fixedPosRadio->setChecked(true);
    m_fixedPosRadio->setStyleSheet("QRadioButton { color: #333; padding: 5px; }");

    m_followCursorRadio = new QRadioButton("🖱 跟随鼠标", this);
    m_followCursorRadio->setStyleSheet("QRadioButton { color: #333; padding: 5px; }");

    m_sequenceRadio = new QRadioButton("📋 序列点击", this);
    m_sequenceRadio->setStyleSheet("QRadioButton { color: #333; padding: 5px; }");

    m_randomRadio = new QRadioButton("🎲 随机区域", this);
    m_randomRadio->setStyleSheet("QRadioButton { color: #333; padding: 5px; }");

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

    // 点击方法（无干扰/模拟鼠标）
    QLabel* clickMethodLabel = new QLabel("点击方法:", this);
    clickMethodLabel->setStyleSheet("color: #333;");
    m_clickMethodCombo = new QComboBox(this);
    m_clickMethodCombo->addItem("无干扰 (推荐)", static_cast<int>(ClickMethod::NoInterference));
    m_clickMethodCombo->addItem("模拟鼠标", static_cast<int>(ClickMethod::SimulateMouse));
    m_clickMethodCombo->setStyleSheet("QComboBox { padding: 5px; border: 1px solid #ddd; border-radius: 4px; background: #fff; }");

    // 点击间隔
    QLabel* intervalLabel = new QLabel("间隔 (ms):", this);
    intervalLabel->setStyleSheet("color: #333;");

    m_intervalSpin = new QSpinBox(this);
    m_intervalSpin->setRange(1, 10000);
    m_intervalSpin->setValue(100);
    m_intervalSpin->setStyleSheet("QSpinBox { padding: 5px; border: 1px solid #ddd; border-radius: 4px; background: #fff; }");

    m_randomJitterCheck = new QCheckBox("随机扰动", this);
    m_randomJitterCheck->setStyleSheet("QCheckBox { color: #333; padding: 5px; }");

    m_jitterRangeSpin = new QSpinBox(this);
    m_jitterRangeSpin->setRange(1, 500);
    m_jitterRangeSpin->setValue(10);
    m_jitterRangeSpin->setSuffix(" ms");
    m_jitterRangeSpin->setEnabled(false);
    m_jitterRangeSpin->setStyleSheet("QSpinBox { padding: 5px; border: 1px solid #ddd; border-radius: 4px; background: #fff; }");

    connect(m_randomJitterCheck, &QCheckBox::toggled, m_jitterRangeSpin, &QSpinBox::setEnabled);

    QHBoxLayout* intervalLayout = new QHBoxLayout();
    intervalLayout->addWidget(m_intervalSpin);
    intervalLayout->addWidget(m_randomJitterCheck);
    intervalLayout->addWidget(m_jitterRangeSpin);

    // 点击次数
    QLabel* countLabel = new QLabel("点击次数:", this);
    countLabel->setStyleSheet("color: #333;");
    m_countSpin = new QSpinBox(this);
    m_countSpin->setRange(-1, 1000000);
    m_countSpin->setValue(-1);
    m_countSpin->setSpecialValueText("无限");
    m_countSpin->setStyleSheet("QSpinBox { padding: 5px; border: 1px solid #ddd; border-radius: 4px; background: #fff; }");

    // 防检测
    m_antiDetectCheck = new QCheckBox("🛡 防检测随机化", this);
    m_antiDetectCheck->setStyleSheet("QCheckBox { color: #333; padding: 5px; }");

    // 窗口置顶
    m_stayOnTopCheck = new QCheckBox("🔝 窗口置顶", this);
    m_stayOnTopCheck->setStyleSheet("QCheckBox { color: #333; padding: 5px; }");
    connect(m_stayOnTopCheck, &QCheckBox::toggled, this, &MainWindow::onStayOnTopToggled);

    layout->addWidget(buttonLabel, 0, 0);
    layout->addWidget(m_buttonCombo, 0, 1);
    layout->addWidget(clickTypeLabel, 1, 0);
    layout->addWidget(m_clickTypeCombo, 1, 1);
    layout->addWidget(clickMethodLabel, 2, 0);
    layout->addWidget(m_clickMethodCombo, 2, 1);
    layout->addWidget(intervalLabel, 3, 0);
    layout->addLayout(intervalLayout, 3, 1);
    layout->addWidget(countLabel, 4, 0);
    layout->addWidget(m_countSpin, 4, 1);
    layout->addWidget(m_antiDetectCheck, 5, 0, 1, 2);
    layout->addWidget(m_stayOnTopCheck, 6, 0, 1, 2);

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

    // 选取坐标按钮
    m_pickPosBtn = new QPushButton("📍 选取坐标", this);
    m_pickPosBtn->setStyleSheet("QPushButton { padding: 10px; background: #2196F3; color: white; border-radius: 6px; border: none; } QPushButton:hover { background: #1976D2; }");
    connect(m_pickPosBtn, &QPushButton::clicked, this, &MainWindow::onPickPositionClicked);
    layout->addWidget(m_pickPosBtn);

    // 序列操作
    QHBoxLayout* seqBtnLayout = new QHBoxLayout();
    m_addSeqBtn = new QPushButton("➕ 添加序列", this);
    m_addSeqBtn->setStyleSheet("QPushButton { padding: 8px; background: #4CAF50; color: white; border-radius: 4px; border: none; } QPushButton:hover { background: #45a049; }");
    m_addSeqBtn->setEnabled(false);
    m_clearSeqBtn = new QPushButton("🗑 清除序列", this);
    m_clearSeqBtn->setStyleSheet("QPushButton { padding: 8px; background: #9E9E9E; color: white; border-radius: 4px; border: none; } QPushButton:hover { background: #757575; }");
    m_clearSeqBtn->setEnabled(false);
    connect(m_addSeqBtn, &QPushButton::clicked, this, &MainWindow::onAddToSequenceClicked);
    connect(m_clearSeqBtn, &QPushButton::clicked, this, &MainWindow::onClearSequenceClicked);
    seqBtnLayout->addWidget(m_addSeqBtn);
    seqBtnLayout->addWidget(m_clearSeqBtn);
    layout->addLayout(seqBtnLayout);

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

    // 序列列表
    m_sequenceList = new QListWidget(this);
    m_sequenceList->setMaximumHeight(120);
    m_sequenceList->setEnabled(false);
    m_sequenceList->setStyleSheet("QListWidget { border: 1px solid #ddd; border-radius: 4px; background: #fafafa; } QListWidget::item { padding: 5px; }");
    layout->addWidget(m_sequenceList);

    return group;
}

ClickConfig MainWindow::getConfig() const
{
    ClickConfig config;
    config.mode = getModeFromConfig();
    config.targetX = m_targetX;
    config.targetY = m_targetY;
    config.buttonType = m_buttonCombo->currentData().toString();
    config.clickType = m_clickTypeCombo->currentData().toString();
    config.clickMethod = static_cast<ClickMethod>(m_clickMethodCombo->currentData().toInt());
    config.intervalBase = m_intervalSpin->value();
    config.useRandomJitter = m_randomJitterCheck->isChecked();
    config.jitterRange = m_jitterRangeSpin->value();
    config.clickCount = m_countSpin->value();
    config.antiDetect = m_antiDetectCheck->isChecked();

    // Get sequence points
    for (int i = 0; i < m_sequenceList->count(); ++i) {
        QString text = m_sequenceList->item(i)->text();
        QRegularExpression re("\\((\\d+), (\\d+)\\)");
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch()) {
            config.sequencePoints.append(QPoint(match.captured(1).toInt(), match.captured(2).toInt()));
        }
    }

    return config;
}

ClickMode MainWindow::getModeFromConfig() const
{
    if (m_fixedPosRadio->isChecked()) return ClickMode::FixedPosition;
    if (m_followCursorRadio->isChecked()) return ClickMode::FollowCursor;
    if (m_sequenceRadio->isChecked()) return ClickMode::Sequence;
    if (m_randomRadio->isChecked()) return ClickMode::RandomArea;
    return ClickMode::Drag;
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
    m_clickEngine->setInterval(config.intervalBase, config.useRandomJitter ? config.jitterRange : 0);
    m_clickEngine->setClickCount(config.clickCount);
    m_clickEngine->setAntiDetect(config.antiDetect);
    m_clickEngine->setClickMethod(config.clickMethod);
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
    if (m_platformAdapter) {
        QPoint pos = m_platformAdapter->getMousePosition();
        m_positionLabel->setText(QString("当前坐标: (%1, %2)").arg(pos.x()).arg(pos.y()));
    }
}

void MainWindow::onPickPositionClicked()
{
    if (m_platformAdapter) {
        QPoint pos = m_platformAdapter->getMousePosition();
        m_targetX = pos.x();
        m_targetY = pos.y();
        m_positionLabel->setText(QString("目标坐标: (%1, %2)").arg(pos.x()).arg(pos.y()));
    }
}

void MainWindow::onAddToSequenceClicked()
{
    if (m_platformAdapter) {
        QPoint pos = m_platformAdapter->getMousePosition();
        QString itemText = QString("(%1, %2)").arg(pos.x()).arg(pos.y());
        m_sequenceList->addItem(itemText);
    }
}

void MainWindow::onClearSequenceClicked()
{
    m_sequenceList->clear();
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

void MainWindow::onClickPerformed(int x, int y)
{
    int total = m_clickEngine->getTotalClicks();
    m_clickCountLabel->setText(QString("点击次数: %1").arg(total));
}

void MainWindow::onEngineFinished()
{
    m_statusLabel->setText("✅ 已完成");
    m_statusLabel->setStyleSheet("font-size: 16px; padding: 15px; background-color: #bbdefb; border-radius: 8px; color: #1565c0;");
    m_clickCountLabel->setText(QString("点击次数: %1").arg(m_clickEngine->getTotalClicks()));
    statusBar()->showMessage("已完成 | 总点击: " + QString::number(m_clickEngine->getTotalClicks()));
}

void MainWindow::onStayOnTopToggled(bool enabled)
{
    m_stayOnTop = enabled;
    setWindowStayOnTop(enabled);
}

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
    QString itemText = QString("(%1, %2)").arg(x).arg(y);
    m_sequenceList->addItem(itemText);
}