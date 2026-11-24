#include "MainWindow.h"
#include "core/WindowsPlatformAdapter.h"
#include <QToolBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QStatusBar>
#include <QGroupBox>
#include <QTimer>
#include <QApplication>
#include <QDebug>
#include <QDialog>
#include <QInputDialog>  // v2.2: for interval setting dialog
#include <QMenu>
#include <QMetaObject>
#include <QFormLayout>
#include <QScrollArea>
#include <QStyle>
#include <QSizePolicy>
#include <QHeaderView>
#include <QSignalBlocker>
#include <QDialogButtonBox>

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
    , m_sequenceTable(nullptr)
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
    // Delay hook installation until the Qt event loop and main window are ready.
    QTimer::singleShot(100, this, &MainWindow::setupHotkeys);
    setupClickEngine();
    setupWindowUpdateTimer();

    // Timer for updating mouse position
    QTimer* mouseTimer = new QTimer(this);
    connect(mouseTimer, &QTimer::timeout, this, &MainWindow::updateMousePosition);
    mouseTimer->start(100);
    connect(m_countSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::updateControls);
    connect(m_sequenceTable, &QTableWidget::currentCellChanged, this, [this]() { updateControls(); });
    connect(m_randomizeCheck, &QCheckBox::toggled, this, &MainWindow::updateControls);
    connect(m_elementTextEdit, &QLineEdit::textChanged, this, [this]() {
        m_hasTargetElement = false;
        m_targetElement = {};
        m_elementInfoLabel->setText(QStringLiteral("修改文本后请重新查找元素。"));
        updateControls();
    });
    setStatus(QStringLiteral("就绪"), QStringLiteral("idle"));
    updateControls();
}

MainWindow::~MainWindow()
{
#ifdef Q_OS_WIN
    m_platformAdapter->unregisterHotkey(1);
    m_platformAdapter->unregisterHotkey(2);
#endif

    if (m_isRecording) {
        m_platformAdapter->stopMouseHook();
        m_isRecording = false;
    }
}

void MainWindow::setupHotkeys()
{
#ifdef Q_OS_WIN
    const bool startStopRegistered = m_platformAdapter->registerHotkey(1, Qt::Key_F6, 0, [this]() {
        QMetaObject::invokeMethod(this, [this]() {
            if (m_clickEngine->isRunning()) {
                onStopClicked();
            } else {
                onStartClicked();
            }
        }, Qt::QueuedConnection);
    });

    const bool pauseRegistered = m_platformAdapter->registerHotkey(2, Qt::Key_F7, 0, [this]() {
        QMetaObject::invokeMethod(this, [this]() {
            onPauseClicked();
        }, Qt::QueuedConnection);
    });

    if (startStopRegistered && pauseRegistered) {
        qDebug() << "全局热键已注册: F6 (开始/停止), F7 (暂停/继续)";
        statusBar()->showMessage("就绪 | 全局热键: F6 开始/停止 | F7 暂停/继续");
    } else {
        const QString message = startStopRegistered || pauseRegistered
            ? "部分全局热键注册失败，请检查系统键盘钩子权限"
            : "全局热键注册失败，请检查系统键盘钩子权限";
        qWarning() << message;
        statusBar()->showMessage(message, 10000);
    }
#endif
}

void MainWindow::setupClickEngine()
{
    connect(m_clickEngine.get(), &ClickEngine::clickPerformed, this, &MainWindow::onClickPerformed);
    connect(m_clickEngine.get(), &ClickEngine::finished, this, &MainWindow::onEngineFinished);
    connect(m_clickEngine.get(), &ClickEngine::windowLost, this, [this](uintptr_t, const QString& title) {
        statusBar()->showMessage(QStringLiteral("目标窗口已关闭：%1").arg(title), 5000);
    });
    connect(m_clickEngine.get(), &ClickEngine::errorOccurred, this, [this](const QString& error) {
        m_errorMessage = error;
        setStatus(QStringLiteral("出错"), QStringLiteral("error"));
        statusBar()->showMessage(error, 5000);
        updateControls();
    });
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::setupUI()
{
    setWindowTitle(QStringLiteral("AutoClicker v") + qApp->applicationVersion());
    setStyleSheet(QStringLiteral(R"(
        QMainWindow { background: #f3f5f9; }
        QWidget { color: #233047; font-family: "Microsoft YaHei UI"; font-size: 9pt; }
        QToolBar { background: #ffffff; border: none; border-bottom: 1px solid #dce2eb; padding: 12px 16px; spacing: 8px; }
        QToolBar::separator { width: 12px; }
        QLabel { background: transparent; }
        QLabel#appTitle { font-size: 18px; font-weight: 600; color: #17253c; }
        QLabel#appSubtitle, QLabel#muted { color: #5e6d82; }
        QLabel#statusBadge { border-radius: 5px; padding: 6px 10px; background: #edf1f6; color: #52627a; font-weight: 600; }
        QLabel#statusBadge[tone="running"] { background: #e8f0ff; color: #1d4ed8; }
        QLabel#statusBadge[tone="paused"] { background: #fff3da; color: #8b5b00; }
        QLabel#statusBadge[tone="recording"], QLabel#statusBadge[tone="error"] { background: #fff0f2; color: #b42335; }
        QLabel#statusBadge[tone="finished"] { background: #e9f6ed; color: #216e3b; }
        QLabel#summary { color: #384b67; font-weight: 600; }
        QGroupBox { background: #ffffff; border: 1px solid #dce2eb; border-radius: 8px; margin-top: 12px; padding: 18px 14px 14px; font-weight: 600; }
        QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 4px; color: #30435f; }
        QPushButton { background: #ffffff; border: 1px solid #ccd5e2; border-radius: 5px; padding: 7px 12px; min-height: 18px; }
        QPushButton:hover { background: #eff4fb; border-color: #8da7cb; }
        QPushButton:pressed { background: #e2ebf7; }
        QPushButton:focus, QComboBox:focus, QSpinBox:focus, QLineEdit:focus, QTableWidget:focus { border: 1px solid #2563eb; }
        QPushButton:disabled { color: #8995a7; background: #f4f6f9; border-color: #e0e5ed; }
        QPushButton#primary { color: #ffffff; background: #2563eb; border-color: #2563eb; font-weight: 600; }
        QPushButton#primary:hover { background: #1d4ed8; }
        QPushButton#primary:disabled { background: #b8c9e8; border-color: #b8c9e8; color: #ffffff; }
        QPushButton#stop { color: #b42335; }
        QPushButton#stop:disabled { color: #8995a7; }
        QPushButton:checked { color: #b42335; background: #fff1f2; border-color: #f2a6af; }
        QComboBox, QSpinBox, QLineEdit { background: #ffffff; border: 1px solid #ccd5e2; border-radius: 4px; padding: 5px 7px; min-height: 18px; selection-background-color: #dbeafe; selection-color: #17253c; }
        QComboBox:disabled, QSpinBox:disabled, QLineEdit:disabled { background: #f4f6f9; color: #8995a7; border-color: #e0e5ed; }
        QComboBox QAbstractItemView { background: #ffffff; color: #233047; selection-background-color: #dbeafe; selection-color: #17253c; }
        QCheckBox { spacing: 6px; padding: 3px 0; }
        QTableWidget { background: #ffffff; alternate-background-color: #f8fafc; border: 1px solid #dce2eb; border-radius: 5px; gridline-color: #edf0f5; selection-background-color: #e8f0ff; selection-color: #163e80; }
        QHeaderView::section { background: #f3f6fb; color: #52627a; border: none; border-bottom: 1px solid #dce2eb; padding: 8px 6px; font-weight: 600; }
        QTableWidget::item { padding: 4px; }
        QScrollArea { background: transparent; border: none; }
        QWidget#settingsPanel { background: transparent; }
        QStatusBar { background: #ffffff; color: #5e6d82; border-top: 1px solid #dce2eb; padding: 3px 10px; }
        QToolTip { background: #233047; color: #ffffff; border: none; padding: 6px; }
    )"));

    setupToolbar();
    setupStatusBar();
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(20, 14, 20, 16);
    layout->setSpacing(12);

    auto* summary = new QHBoxLayout();
    m_modeLabel = new QLabel(QStringLiteral("执行模式：固定位置"), this);
    m_modeLabel->setObjectName(QStringLiteral("summary"));
    m_clickCountLabel = new QLabel(QStringLiteral("已执行 0 次"), this);
    m_clickCountLabel->setObjectName(QStringLiteral("summary"));
    summary->addWidget(m_modeLabel);
    summary->addStretch();
    summary->addWidget(m_clickCountLabel);
    layout->addLayout(summary);
    m_validationLabel = new QLabel(this);
    m_validationLabel->setObjectName(QStringLiteral("muted"));
    m_validationLabel->setWordWrap(true);
    layout->addWidget(m_validationLabel);

    auto* body = new QHBoxLayout();
    body->setSpacing(16);
    auto* settings = new QWidget(this);
    settings->setObjectName(QStringLiteral("settingsPanel"));
    auto* settingsLayout = new QVBoxLayout(settings);
    settingsLayout->setContentsMargins(0, 0, 4, 0);
    settingsLayout->setSpacing(12);
    m_configGroup = createConfigGroupBox();
    m_targetGroup = createTargetWindowGroupBox();
    settingsLayout->addWidget(m_configGroup);
    settingsLayout->addWidget(m_targetGroup);
    settingsLayout->addStretch();
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumWidth(270);
    scroll->setMaximumWidth(340);
    scroll->setWidget(settings);
    body->addWidget(scroll, 3);
    body->addWidget(createPositionGroupBox(), 6);
    layout->addLayout(body, 1);
    setCentralWidget(central);
    setWindowStayOnTop(true);
}

void MainWindow::setupToolbar()
{
    auto* toolbar = addToolBar(QStringLiteral("执行控制"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setIconSize(QSize(16, 16));
    auto* brand = new QWidget(this);
    auto* brandLayout = new QVBoxLayout(brand);
    brandLayout->setContentsMargins(0, 0, 0, 0);
    brandLayout->setSpacing(2);
    auto* title = new QLabel(QStringLiteral("AutoClicker"), brand);
    title->setObjectName(QStringLiteral("appTitle"));
    auto* subtitle = new QLabel(QStringLiteral("自动点击工具 · v") + qApp->applicationVersion(), brand);
    subtitle->setObjectName(QStringLiteral("appSubtitle"));
    brandLayout->addWidget(title);
    brandLayout->addWidget(subtitle);
    toolbar->addWidget(brand);
    auto* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);
    m_statusLabel = new QLabel(QStringLiteral("就绪"), this);
    m_statusLabel->setObjectName(QStringLiteral("statusBadge"));
    m_statusLabel->setMinimumWidth(64);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    toolbar->addWidget(m_statusLabel);
    toolbar->addSeparator();

    m_startBtn = new QPushButton(style()->standardIcon(QStyle::SP_MediaPlay), QStringLiteral("开始 · F6"), this);
    m_startBtn->setObjectName(QStringLiteral("primary"));
    m_pauseBtn = new QPushButton(style()->standardIcon(QStyle::SP_MediaPause), QStringLiteral("暂停 · F7"), this);
    m_stopBtn = new QPushButton(style()->standardIcon(QStyle::SP_MediaStop), QStringLiteral("停止"), this);
    m_stopBtn->setObjectName(QStringLiteral("stop"));
    for (auto* button : {m_startBtn, m_pauseBtn, m_stopBtn}) {
        button->setMinimumWidth(88);
        toolbar->addWidget(button);
    }
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
}

void MainWindow::setupStatusBar()
{
    statusBar()->setSizeGripEnabled(true);
    statusBar()->showMessage(QStringLiteral("F6 开始 / 停止    F7 暂停 / 继续"));
}

QGroupBox* MainWindow::createConfigGroupBox()
{
    auto* group = new QGroupBox(QStringLiteral("点击参数"), this);
    auto* layout = new QFormLayout(group);
    layout->setSpacing(10);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_buttonCombo = new QComboBox(group);
    m_buttonCombo->addItem(QStringLiteral("左键"), QStringLiteral("left"));
    m_buttonCombo->addItem(QStringLiteral("右键"), QStringLiteral("right"));
    m_buttonCombo->addItem(QStringLiteral("中键"), QStringLiteral("middle"));
    m_clickTypeCombo = new QComboBox(group);
    m_clickTypeCombo->addItem(QStringLiteral("单击"), QStringLiteral("single"));
    m_clickTypeCombo->addItem(QStringLiteral("双击"), QStringLiteral("double"));
    m_clickTypeCombo->addItem(QStringLiteral("三击"), QStringLiteral("triple"));
    m_clickTypeCombo->addItem(QStringLiteral("长按"), QStringLiteral("hold"));
    m_clickMethodCombo = new QComboBox(group);
    m_clickMethodCombo->addItem(QStringLiteral("无干扰"), static_cast<int>(ClickMethod::NoInterference));
    m_clickMethodCombo->addItem(QStringLiteral("模拟鼠标"), static_cast<int>(ClickMethod::SimulateMouse));
    m_clickMethodCombo->setToolTip(QStringLiteral("无干扰：向窗口发送消息；部分回退路径仍可能移动鼠标，兼容性取决于目标应用。\n模拟鼠标：移动鼠标并点击，会影响当前鼠标操作。"));
    m_intervalSpin = new QSpinBox(group);
    m_intervalSpin->setRange(1, 10000);
    m_intervalSpin->setValue(100);
    m_intervalSpin->setSuffix(QStringLiteral(" ms"));
    m_countSpin = new QSpinBox(group);
    m_countSpin->setRange(-1, 1000000);
    m_countSpin->setValue(0);
    m_countSpin->setSpecialValueText(QStringLiteral("无限"));
    m_countSpin->setToolTip(QStringLiteral("0：不执行；-1：无限；正数：执行指定次数。"));
    layout->addRow(QStringLiteral("鼠标按键"), m_buttonCombo);
    layout->addRow(QStringLiteral("点击动作"), m_clickTypeCombo);
    layout->addRow(QStringLiteral("点击方法"), m_clickMethodCombo);
    layout->addRow(QStringLiteral("全局间隔"), m_intervalSpin);
    layout->addRow(QStringLiteral("执行次数"), m_countSpin);
    m_randomizeCheck = new QCheckBox(QStringLiteral("随机化间隔与位置"), group);
    m_randomizeCheck->setToolTip(QStringLiteral("间隔增加随机扰动，位置偏移 ±5 像素。"));
    m_jitterRangeSpin = new QSpinBox(group);
    m_jitterRangeSpin->setRange(1, 500);
    m_jitterRangeSpin->setValue(10);
    m_jitterRangeSpin->setPrefix(QStringLiteral("± "));
    m_jitterRangeSpin->setSuffix(QStringLiteral(" ms"));
    m_jitterRangeSpin->setEnabled(false);
    layout->addRow(m_randomizeCheck);
    layout->addRow(QStringLiteral("间隔扰动"), m_jitterRangeSpin);
    connect(m_randomizeCheck, &QCheckBox::toggled, m_jitterRangeSpin, &QSpinBox::setEnabled);
    return group;
}

QGroupBox* MainWindow::createPositionGroupBox()
{
    auto* group = new QGroupBox(QStringLiteral("点击序列"), this);
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(12);
    auto* heading = new QHBoxLayout();
    m_positionLabel = new QLabel(QStringLiteral("鼠标坐标：(0, 0)"), group);
    m_positionLabel->setObjectName(QStringLiteral("muted"));
    m_pointCountLabel = new QLabel(QStringLiteral("0 个点位"), group);
    m_pointCountLabel->setObjectName(QStringLiteral("summary"));
    heading->addWidget(m_positionLabel);
    heading->addStretch();
    heading->addWidget(m_pointCountLabel);
    layout->addLayout(heading);
    auto* hint = new QLabel(QStringLiteral("按列表顺序循环点击。双击编辑坐标，右键设置独立间隔。"), group);
    hint->setObjectName(QStringLiteral("muted"));
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* manual = new QHBoxLayout();
    m_manualXSpin = new QSpinBox(group);
    m_manualYSpin = new QSpinBox(group);
    for (auto* spin : {m_manualXSpin, m_manualYSpin}) {
        spin->setRange(0, 9999);
        spin->setMinimumWidth(80);
    }
    auto* xLabel = new QLabel(QStringLiteral("X"), group);
    auto* yLabel = new QLabel(QStringLiteral("Y"), group);
    xLabel->setBuddy(m_manualXSpin);
    yLabel->setBuddy(m_manualYSpin);
    m_manualXSpin->setAccessibleName(QStringLiteral("手动 X 坐标"));
    m_manualYSpin->setAccessibleName(QStringLiteral("手动 Y 坐标"));
    m_addManualBtn = new QPushButton(QStringLiteral("添加点位"), group);
    m_addManualBtn->setToolTip(QStringLiteral("添加绝对屏幕坐标，手动点位使用左键。"));
    connect(m_addManualBtn, &QPushButton::clicked, this, &MainWindow::onAddManualCoordinateClicked);
    manual->addWidget(xLabel);
    manual->addWidget(m_manualXSpin, 1);
    manual->addWidget(yLabel);
    manual->addWidget(m_manualYSpin, 1);
    manual->addWidget(m_addManualBtn);
    layout->addLayout(manual);
    auto* recording = new QHBoxLayout();
    m_recordBtn = new QPushButton(QStringLiteral("开始录制"), group);
    m_recordBtn->setCheckable(true);
    m_recordStatusLabel = new QLabel(QStringLiteral("未录制"), group);
    m_recordStatusLabel->setObjectName(QStringLiteral("muted"));
    recording->addWidget(m_recordBtn);
    recording->addWidget(m_recordStatusLabel, 1);
    connect(m_recordBtn, &QPushButton::toggled, this, &MainWindow::onRecordToggled);
    layout->addLayout(recording);
    m_sequenceTable = new QTableWidget(0, 6, group);
    m_sequenceTable->setObjectName(QStringLiteral("sequenceTable"));
    m_sequenceTable->setAccessibleName(QStringLiteral("点击点位表格"));
    m_sequenceTable->setHorizontalHeaderLabels({QStringLiteral("序号"), QStringLiteral("按键"), QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("间隔"), QStringLiteral("绑定窗口")});
    m_sequenceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sequenceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_sequenceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sequenceTable->setAlternatingRowColors(true);
    m_sequenceTable->setSortingEnabled(false);
    m_sequenceTable->verticalHeader()->hide();
    m_sequenceTable->verticalHeader()->setDefaultSectionSize(34);
    m_sequenceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_sequenceTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_sequenceTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_sequenceTable->setMinimumHeight(160);
    connect(m_sequenceTable, &QTableWidget::customContextMenuRequested, this, &MainWindow::onSequenceContextMenuRequested);
    connect(m_sequenceTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onSequenceItemDoubleClicked);
    layout->addWidget(m_sequenceTable, 1);
    auto* editing = new QHBoxLayout();
    m_deleteSeqBtn = new QPushButton(style()->standardIcon(QStyle::SP_TrashIcon), QStringLiteral("删除"), group);
    m_moveUpBtn = new QPushButton(style()->standardIcon(QStyle::SP_ArrowUp), QStringLiteral("上移"), group);
    m_moveDownBtn = new QPushButton(style()->standardIcon(QStyle::SP_ArrowDown), QStringLiteral("下移"), group);
    m_clearSeqBtn = new QPushButton(QStringLiteral("清空"), group);
    editing->addWidget(m_deleteSeqBtn);
    editing->addWidget(m_moveUpBtn);
    editing->addWidget(m_moveDownBtn);
    editing->addStretch();
    editing->addWidget(m_clearSeqBtn);
    connect(m_deleteSeqBtn, &QPushButton::clicked, this, &MainWindow::onDeleteSequenceItemClicked);
    connect(m_moveUpBtn, &QPushButton::clicked, this, &MainWindow::onMoveSequenceItemUp);
    connect(m_moveDownBtn, &QPushButton::clicked, this, &MainWindow::onMoveSequenceItemDown);
    connect(m_clearSeqBtn, &QPushButton::clicked, this, &MainWindow::onClearSequenceClicked);
    layout->addLayout(editing);
    return group;
}

QGroupBox* MainWindow::createTargetWindowGroupBox()
{
    auto* group = new QGroupBox(QStringLiteral("目标窗口"), this);
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(10);
    m_currentWindowLabel = new QLabel(QStringLiteral("当前窗口：等待执行"), group);
    m_currentWindowLabel->setObjectName(QStringLiteral("muted"));
    m_currentWindowLabel->setWordWrap(true);
    layout->addWidget(m_currentWindowLabel);
    auto* windowRow = new QHBoxLayout();
    m_targetWindowCombo = new QComboBox(group);
    m_targetWindowCombo->setMinimumContentsLength(10);
    m_targetWindowCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_targetWindowCombo->addItem(QStringLiteral("自动检测"), qulonglong(0));
    m_targetWindowCombo->setAccessibleName(QStringLiteral("绑定目标窗口"));
    m_refreshWindowsBtn = new QPushButton(style()->standardIcon(QStyle::SP_BrowserReload), QString(), group);
    m_refreshWindowsBtn->setToolTip(QStringLiteral("刷新窗口列表"));
    m_refreshWindowsBtn->setAccessibleName(QStringLiteral("刷新窗口列表"));
    windowRow->addWidget(m_targetWindowCombo, 1);
    windowRow->addWidget(m_refreshWindowsBtn);
    layout->addLayout(windowRow);
    connect(m_refreshWindowsBtn, &QPushButton::clicked, this, &MainWindow::onRefreshWindowsClicked);
    connect(m_targetWindowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onTargetWindowSelected);
    auto* label = new QLabel(QStringLiteral("元素文本"), group);
    m_elementTextEdit = new QLineEdit(group);
    label->setBuddy(m_elementTextEdit);
    m_elementTextEdit->setPlaceholderText(QStringLiteral("输入原生控件上的文字"));
    layout->addWidget(label);
    auto* elementRow = new QHBoxLayout();
    m_findElementBtn = new QPushButton(QStringLiteral("查找"), group);
    elementRow->addWidget(m_elementTextEdit, 1);
    elementRow->addWidget(m_findElementBtn);
    layout->addLayout(elementRow);
    connect(m_findElementBtn, &QPushButton::clicked, this, &MainWindow::onFindElementClicked);
    m_elementInfoLabel = new QLabel(QStringLiteral("录制点位使用各自记录的窗口绑定。"), group);
    m_elementInfoLabel->setObjectName(QStringLiteral("muted"));
    m_elementInfoLabel->setWordWrap(true);
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
    config.clickMethod = static_cast<ClickMethod>(m_clickMethodCombo->currentData().toInt());  // v2.2: restored
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

    // Use user-selected click method (v2.2: restored for game compatibility)
    m_clickEngine->setClickMethod(config.clickMethod);

    // Always synchronize the UI target, including clearing a previous element.
    m_clickEngine->setTargetWindow(m_targetWindowId);
    m_clickEngine->setTargetElement(m_hasTargetElement ? m_targetElement : ElementInfo{});
}

void MainWindow::updateMousePosition()
{
    if (m_platformAdapter) {
        QPoint pos = m_platformAdapter->getMousePosition();
        m_positionLabel->setText(QStringLiteral("鼠标坐标：(%1, %2)").arg(pos.x()).arg(pos.y()));
    }
}

void MainWindow::onClearSequenceClicked()
{
    if (!canEditSequence()) return;
    m_sequencePoints.clear();
    updateSequenceListDisplay();
}

void MainWindow::onStartClicked()
{
    if (m_clickEngine->isRunning()) return;
    if (QApplication::activeModalWidget() || QApplication::activePopupWidget()) return;
    const QString reason = startBlockReason();
    if (!reason.isEmpty()) {
        statusBar()->showMessage(reason, 5000);
        updateControls();
        return;
    }
    m_errorMessage.clear();
    applyConfigToEngine();
    m_clickEngine->start();
    setStatus(QStringLiteral("运行中"), QStringLiteral("running"));
    m_clickCountLabel->setText(QStringLiteral("已执行 0 次"));
    statusBar()->showMessage(QStringLiteral("运行中 · F6 停止，F7 暂停"));
    updateControls();
}

void MainWindow::onStopClicked()
{
    if (!m_clickEngine->isRunning()) return;
    m_clickEngine->stop();
    setStatus(QStringLiteral("已停止"), QStringLiteral("idle"));
    m_clickCountLabel->setText(QStringLiteral("已执行 %1 次").arg(m_clickEngine->getTotalClicks()));
    statusBar()->showMessage(QStringLiteral("已停止 · 总执行 %1 次").arg(m_clickEngine->getTotalClicks()));
    updateControls();
}

void MainWindow::onPauseClicked()
{
    if (!m_clickEngine->isRunning()) return;
    if (m_clickEngine->isPaused()) {
        m_clickEngine->resume();
        setStatus(QStringLiteral("运行中"), QStringLiteral("running"));
        statusBar()->showMessage(QStringLiteral("运行中 · F6 停止，F7 暂停"));
    } else {
        m_clickEngine->pause();
        setStatus(QStringLiteral("已暂停"), QStringLiteral("paused"));
        statusBar()->showMessage(QStringLiteral("已暂停 · F7 继续，F6 停止"));
    }
    updateControls();
}

void MainWindow::onClickPerformed(int x, int y, uintptr_t windowId)
{
    Q_UNUSED(x);
    Q_UNUSED(y);
    m_clickCountLabel->setText(QStringLiteral("已执行 %1 次").arg(m_clickEngine->getTotalClicks()));
    m_lastClickedWindowId = windowId;
}

void MainWindow::onEngineFinished()
{
    setStatus(QStringLiteral("已完成"), QStringLiteral("finished"));
    m_clickCountLabel->setText(QStringLiteral("已执行 %1 次").arg(m_clickEngine->getTotalClicks()));
    statusBar()->showMessage(QStringLiteral("已完成 · 总执行 %1 次").arg(m_clickEngine->getTotalClicks()));
    updateControls();
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
    if (!canEditSequence()) return;
    int x = m_manualXSpin->value();
    int y = m_manualYSpin->value();

    // Create a SequencePoint without window binding
    SequencePoint point{};
    point.screenX = x;
    point.screenY = y;
    point.button = RecordedButton::Left;  // Default to left button
    point.windowId = 0;  // No window binding

    m_sequencePoints.append(point);
    updateSequenceListDisplay();
}

void MainWindow::onRecordToggled(bool enabled)
{
    if (enabled) {
        if (!canEditSequence() || QApplication::activeModalWidget() || QApplication::activePopupWidget()) {
            const QSignalBlocker blocker(m_recordBtn);
            m_recordBtn->setChecked(false);
            return;
        }
        m_lastRecordTime = QDateTime();
        const bool success = m_platformAdapter->startMouseHook([this](const RecordedClick& click) {
            onMouseClickRecorded(click);
        });
        if (!success) {
            const QSignalBlocker blocker(m_recordBtn);
            m_recordBtn->setChecked(false);
            m_isRecording = false;
            m_errorMessage = QStringLiteral("鼠标录制钩子安装失败，请检查权限后重试。");
            m_recordStatusLabel->setText(QStringLiteral("录制失败"));
            setStatus(QStringLiteral("出错"), QStringLiteral("error"));
            updateControls();
            return;
        }
        m_isRecording = true;
        m_errorMessage.clear();
        m_recordBtn->setText(QStringLiteral("停止录制"));
        m_recordStatusLabel->setText(QStringLiteral("录制中 · %1 个点位").arg(m_sequencePoints.size()));
        setStatus(QStringLiteral("录制中"), QStringLiteral("recording"));
        statusBar()->showMessage(QStringLiteral("录制中 · 在目标窗口点击，再点“停止录制”结束"));
    } else {
        m_platformAdapter->stopMouseHook();
        m_isRecording = false;
        m_recordBtn->setText(QStringLiteral("开始录制"));
        m_recordStatusLabel->setText(QStringLiteral("共 %1 个点位").arg(m_sequencePoints.size()));
        setStatus(QStringLiteral("就绪"), QStringLiteral("idle"));
        statusBar()->showMessage(QStringLiteral("录制结束 · 共 %1 个点位").arg(m_sequencePoints.size()));
    }
    updateControls();
}

void MainWindow::onMouseClickRecorded(const RecordedClick& click)
{
    if (!m_isRecording) return;

    // Create SequencePoint with full information
    SequencePoint point{};
    point.screenX = click.x;
    point.screenY = click.y;
    point.button = click.button;

    // v2.2: Calculate interval from last recorded point
    QDateTime now = QDateTime::currentDateTime();
    if (!m_sequencePoints.isEmpty() && m_lastRecordTime.isValid()) {
        // Calculate time difference in milliseconds
        qint64 elapsedMs = m_lastRecordTime.msecsTo(now);
        point.intervalMs = static_cast<int>(elapsedMs);
        if (point.intervalMs < 0) point.intervalMs = 0;  // Sanity check
    }
    m_lastRecordTime = now;

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

    m_recordStatusLabel->setText(QStringLiteral("录制中 · %1 个点位").arg(m_sequencePoints.size()));
}

void MainWindow::setupWindowUpdateTimer()
{
    m_windowUpdateTimer = new QTimer(this);
    connect(m_windowUpdateTimer, &QTimer::timeout, this, &MainWindow::updateCurrentWindow);
    m_windowUpdateTimer->start(1000);  // Update every second
}

void MainWindow::updateCurrentWindow()
{
    uintptr_t id = m_clickEngine->isRunning() ? m_clickEngine->getCurrentTargetWindowId() : m_targetWindowId;
    if (!id) id = m_lastClickedWindowId;
    if (!id) {
        m_currentWindowLabel->setText(QStringLiteral("当前窗口：等待执行"));
        return;
    }
    const QString title = QString::fromStdWString(m_platformAdapter->getWindowTitleW(id));
    const QString state = m_platformAdapter->isWindowMinimized(id) ? QStringLiteral("（最小化）") : QString();
    m_currentWindowLabel->setText(QStringLiteral("当前窗口：%1%2").arg(title.isEmpty() ? QStringLiteral("不可用 / 无标题") : title.left(30), state));
    m_currentWindowLabel->setToolTip(title);
}

void MainWindow::onRefreshWindowsClicked()
{
    if (!canEditSequence()) return;
    const uintptr_t previous = m_targetWindowId;
    {
        const QSignalBlocker blocker(m_targetWindowCombo);
        m_targetWindowCombo->clear();
        m_targetWindowCombo->addItem(QStringLiteral("自动检测"), qulonglong(0));
        for (const auto& window : m_platformAdapter->listWindows()) {
            if (window.id == static_cast<uintptr_t>(winId())) continue;
            const QString title = QString::fromStdWString(window.title);
            m_targetWindowCombo->addItem(title.left(40), static_cast<qulonglong>(window.id));
            m_targetWindowCombo->setItemData(m_targetWindowCombo->count() - 1, title, Qt::ToolTipRole);
        }
        const int index = m_targetWindowCombo->findData(static_cast<qulonglong>(previous));
        m_targetWindowCombo->setCurrentIndex(index < 0 ? 0 : index);
    }
    const uintptr_t selected = static_cast<uintptr_t>(m_targetWindowCombo->currentData().toULongLong());
    if (selected != previous) onTargetWindowSelected(m_targetWindowCombo->currentIndex());
    statusBar()->showMessage(QStringLiteral("窗口列表已更新 · %1 个目标").arg(m_targetWindowCombo->count() - 1), 4000);
    updateControls();
}

void MainWindow::onTargetWindowSelected(int index)
{
    if (index < 0 || !canEditSequence()) return;
    m_targetWindowId = static_cast<uintptr_t>(m_targetWindowCombo->currentData().toULongLong());
    m_hasTargetElement = false;
    m_targetElement = {};
    m_elementInfoLabel->setText(m_targetWindowId == 0
        ? QStringLiteral("查找元素前请先刷新并选择窗口。")
        : QStringLiteral("已选择窗口；录制点位仍使用各自的绑定。"));
    m_errorMessage.clear();
    updateCurrentWindow();
    updateControls();
}

void MainWindow::onFindElementClicked()
{
    if (!canEditSequence() || !m_targetWindowId || m_elementTextEdit->text().trimmed().isEmpty()) return;
    m_targetElement = m_platformAdapter->findElementByText(m_targetWindowId, m_elementTextEdit->text().toStdWString());
    m_hasTargetElement = m_targetElement.windowId != 0;
    m_errorMessage.clear();
    m_elementInfoLabel->setText(m_hasTargetElement
        ? QStringLiteral("已定位“%1”。执行时优先点击此元素。修改文本或切换窗口可取消定位。").arg(QString::fromStdWString(m_targetElement.text).left(20))
        : QStringLiteral("未找到匹配的原生控件。自绘界面和网页内部元素可能无法定位。"));
    updateControls();
}

void MainWindow::onDeleteSequenceItemClicked()
{
    if (!canEditSequence()) return;
    const int row = m_sequenceTable->currentRow();
    if (row < 0 || row >= m_sequencePoints.size()) return;
    m_sequencePoints.removeAt(row);
    updateSequenceListDisplay();
    if (!m_sequencePoints.isEmpty()) {
        m_sequenceTable->selectRow(qMin(row, static_cast<int>(m_sequencePoints.size()) - 1));
    }
}

void MainWindow::onMoveSequenceItemUp()
{
    if (!canEditSequence()) return;
    const int row = m_sequenceTable->currentRow();
    if (row <= 0 || row >= m_sequencePoints.size()) return;
    m_sequencePoints.swapItemsAt(row, row - 1);
    updateSequenceListDisplay();
    m_sequenceTable->selectRow(row - 1);
    m_sequenceTable->scrollToItem(m_sequenceTable->item(row - 1, 0));
}

void MainWindow::onMoveSequenceItemDown()
{
    if (!canEditSequence()) return;
    const int row = m_sequenceTable->currentRow();
    if (row < 0 || row >= m_sequencePoints.size() - 1) return;
    m_sequencePoints.swapItemsAt(row, row + 1);
    updateSequenceListDisplay();
    m_sequenceTable->selectRow(row + 1);
    m_sequenceTable->scrollToItem(m_sequenceTable->item(row + 1, 0));
}

void MainWindow::onSequenceItemDoubleClicked(int row, int column)
{
    if (!canEditSequence()) return;
    Q_UNUSED(column);
    if (row < 0 || row >= m_sequencePoints.size()) return;
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("编辑坐标"));
    auto* layout = new QFormLayout(&dialog);
    auto* xSpin = new QSpinBox(&dialog);
    auto* ySpin = new QSpinBox(&dialog);
    xSpin->setRange(0, 9999);
    ySpin->setRange(0, 9999);
    xSpin->setValue(m_sequencePoints.at(row).screenX);
    ySpin->setValue(m_sequencePoints.at(row).screenY);
    layout->addRow(QStringLiteral("屏幕 X"), xSpin);
    layout->addRow(QStringLiteral("屏幕 Y"), ySpin);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addRow(buttons);
    m_editingPoint = true;
    updateControls();
    const bool accepted = dialog.exec() == QDialog::Accepted;
    m_editingPoint = false;
    updateControls();
    if (!accepted || row >= m_sequencePoints.size()) return;
    SequencePoint& point = m_sequencePoints[row];
    point.screenX = xSpin->value();
    point.screenY = ySpin->value();
    if (point.windowId != 0) {
        const WindowInfo info = m_platformAdapter->getWindowInfo(point.windowId);
        point.relX = point.screenX - info.x;
        point.relY = point.screenY - info.y;
    }
    updateSequenceListDisplay();
    m_sequenceTable->selectRow(row);
}

void MainWindow::onSequenceContextMenuRequested(const QPoint& pos)
{
    if (!canEditSequence()) return;
    const int row = m_sequenceTable->rowAt(pos.y());
    if (row < 0 || row >= m_sequencePoints.size()) return;
    m_sequenceTable->selectRow(row);
    QMenu menu(this);
    QAction* edit = menu.addAction(QStringLiteral("编辑坐标"));
    QAction* interval = menu.addAction(QStringLiteral("设置间隔"));
    QAction* remove = menu.addAction(QStringLiteral("删除"));
    menu.addSeparator();
    QAction* up = menu.addAction(QStringLiteral("上移"));
    QAction* down = menu.addAction(QStringLiteral("下移"));
    up->setEnabled(row > 0);
    down->setEnabled(row + 1 < m_sequencePoints.size());
    m_editingPoint = true;
    updateControls();
    QAction* chosen = menu.exec(m_sequenceTable->viewport()->mapToGlobal(pos));
    m_editingPoint = false;
    updateControls();
    if (chosen == edit) {
        onSequenceItemDoubleClicked(row, 0);
    } else if (chosen == remove) {
        onDeleteSequenceItemClicked();
    } else if (chosen == up) {
        onMoveSequenceItemUp();
    } else if (chosen == down) {
        onMoveSequenceItemDown();
    } else if (chosen == interval) {
        QInputDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("设置间隔"));
        dialog.setLabelText(QStringLiteral("点击后等待（毫秒）；0 表示使用全局间隔"));
        dialog.setInputMode(QInputDialog::IntInput);
        dialog.setIntRange(0, 60000);
        dialog.setIntValue(m_sequencePoints.at(row).intervalMs);
        dialog.setIntStep(100);
        dialog.setOkButtonText(QStringLiteral("确定"));
        dialog.setCancelButtonText(QStringLiteral("取消"));
        m_editingPoint = true;
        updateControls();
        const bool accepted = dialog.exec() == QDialog::Accepted;
        m_editingPoint = false;
        updateControls();
        if (accepted && row < m_sequencePoints.size()) {
            m_sequencePoints[row].intervalMs = dialog.intValue();
            updateSequenceListDisplay();
        }
    }
}

void MainWindow::updateSequenceListDisplay()
{
    const int selected = m_sequenceTable->currentRow();
    const QSignalBlocker blocker(m_sequenceTable);
    m_sequenceTable->clearContents();
    m_sequenceTable->setRowCount(static_cast<int>(m_sequencePoints.size()));
    for (int row = 0; row < m_sequencePoints.size(); ++row) {
        const SequencePoint& point = m_sequencePoints.at(row);
        const QString button = point.button == RecordedButton::Left ? QStringLiteral("左键")
            : point.button == RecordedButton::Right ? QStringLiteral("右键") : QStringLiteral("中键");
        const QString interval = point.intervalMs > 0 ? QString::number(point.intervalMs) + QStringLiteral(" ms") : QStringLiteral("全局");
        const QString window = point.windowId == 0 ? QStringLiteral("无绑定")
            : point.windowTitle.empty() ? QStringLiteral("无标题窗口") : QString::fromStdWString(point.windowTitle);
        const QStringList values = {QString::number(row + 1), button, QString::number(point.screenX), QString::number(point.screenY), interval, window};
        for (int column = 0; column < values.size(); ++column) {
            auto* item = new QTableWidgetItem(values.at(column));
            item->setToolTip(values.at(column));
            if (column < 5) item->setTextAlignment(Qt::AlignCenter);
            m_sequenceTable->setItem(row, column, item);
        }
    }
    if (selected >= 0 && !m_sequencePoints.isEmpty()) {
        m_sequenceTable->selectRow(qMin(selected, static_cast<int>(m_sequencePoints.size()) - 1));
    }
    m_pointCountLabel->setText(QStringLiteral("%1 个点位").arg(m_sequencePoints.size()));
    m_recordStatusLabel->setText(m_isRecording ? QStringLiteral("录制中 · %1 个点位").arg(m_sequencePoints.size()) : QStringLiteral("共 %1 个点位").arg(m_sequencePoints.size()));
    updateControls();
}

void MainWindow::setStatus(const QString& text, const QString& tone)
{
    m_statusLabel->setText(text);
    m_statusLabel->setProperty("tone", tone);
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
}

bool MainWindow::canEditSequence() const
{
    return !m_isRecording && !m_editingPoint && !m_clickEngine->isRunning();
}

QString MainWindow::startBlockReason() const
{
    if (m_isRecording) return QStringLiteral("请先停止录制，再开始执行。");
    if (m_editingPoint) return QStringLiteral("请先结束点位编辑。");
    if (m_sequencePoints.isEmpty() && !m_hasTargetElement) return QStringLiteral("请先添加或录制点位，也可选择窗口后查找有效元素。");
    if (m_countSpin->value() == 0) return QStringLiteral("执行次数为 0：请设为正数，或设为 -1（无限）。");
    return {};
}

void MainWindow::updateControls()
{
    const bool running = m_clickEngine->isRunning();
    const bool editable = canEditSequence();
    const int selected = m_sequenceTable->currentRow();
    const bool validRow = selected >= 0 && selected < m_sequencePoints.size();
    const QString reason = startBlockReason();
    m_startBtn->setEnabled(!running && reason.isEmpty());
    m_startBtn->setToolTip(reason.isEmpty() ? QStringLiteral("F6 开始执行") : reason);
    m_pauseBtn->setEnabled(running);
    m_pauseBtn->setText(m_clickEngine->isPaused() ? QStringLiteral("继续 · F7") : QStringLiteral("暂停 · F7"));
    m_pauseBtn->setIcon(style()->standardIcon(m_clickEngine->isPaused() ? QStyle::SP_MediaPlay : QStyle::SP_MediaPause));
    m_stopBtn->setEnabled(running);
    m_configGroup->setEnabled(editable);
    m_targetGroup->setEnabled(editable);
    m_buttonCombo->setEnabled(editable && (m_sequencePoints.isEmpty() || m_hasTargetElement));
    m_buttonCombo->setToolTip(QStringLiteral("序列使用每个点位各自的按键；手动点位沿用左键。"));
    m_jitterRangeSpin->setEnabled(editable && m_randomizeCheck->isChecked());
    m_findElementBtn->setEnabled(editable && m_targetWindowId != 0 && !m_elementTextEdit->text().trimmed().isEmpty());
    m_manualXSpin->setEnabled(editable);
    m_manualYSpin->setEnabled(editable);
    m_addManualBtn->setEnabled(editable);
    m_recordBtn->setEnabled(!running && !m_editingPoint);
    m_sequenceTable->setEnabled(!running && !m_editingPoint);
    m_deleteSeqBtn->setEnabled(editable && validRow);
    m_moveUpBtn->setEnabled(editable && validRow && selected > 0);
    m_moveDownBtn->setEnabled(editable && validRow && selected + 1 < m_sequencePoints.size());
    m_clearSeqBtn->setEnabled(editable && !m_sequencePoints.isEmpty());
    m_modeLabel->setText(m_hasTargetElement ? QStringLiteral("执行模式：元素定位（优先于序列）")
        : m_sequencePoints.isEmpty() ? QStringLiteral("执行模式：待配置") : QStringLiteral("执行模式：序列点击"));
    if (!m_errorMessage.isEmpty()) {
        m_validationLabel->setText(m_errorMessage);
    } else if (running) {
        m_validationLabel->setText(m_clickEngine->isPaused() ? QStringLiteral("已暂停；编辑已锁定。F7 继续，F6 停止。") : QStringLiteral("执行中；参数和点位编辑已锁定。F6 停止，F7 暂停。"));
    } else {
        m_validationLabel->setText(reason.isEmpty() ? QStringLiteral("配置已就绪。F6 开始 / 停止，F7 暂停 / 继续。") : reason);
    }
}
