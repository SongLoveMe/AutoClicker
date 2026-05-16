#include "MainWindow.h"
#include <QToolBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_startBtn(nullptr)
    , m_stopBtn(nullptr)
    , m_pauseBtn(nullptr)
    , m_statusLabel(nullptr)
    , m_isRunning(false)
    , m_isPaused(false)
{
    setupUI();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    setupToolbar();
    setupStatusBar();

    // Central widget
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    // Status indicator
    m_statusLabel = new QLabel("Ready", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("font-size: 14px; padding: 10px;");
    mainLayout->addWidget(m_statusLabel);

    // Placeholder for future widgets
    QLabel* placeholder = new QLabel("Click configuration panel will be here", this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("color: gray; font-size: 12px;");
    mainLayout->addWidget(placeholder);

    mainLayout->addStretch();
    setCentralWidget(centralWidget);
}

void MainWindow::setupToolbar()
{
    QToolBar* toolbar = addToolBar("Main Toolbar");
    toolbar->setMovable(false);

    m_startBtn = new QPushButton("Start", this);
    m_startBtn->setStyleSheet("background-color: #4CAF50; color: white; padding: 5px 15px;");
    m_stopBtn = new QPushButton("Stop", this);
    m_stopBtn->setStyleSheet("background-color: #f44336; color: white; padding: 5px 15px;");
    m_pauseBtn = new QPushButton("Pause", this);
    m_pauseBtn->setStyleSheet("background-color: #FF9800; color: white; padding: 5px 15px;");

    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::onPauseClicked);

    toolbar->addWidget(m_startBtn);
    toolbar->addWidget(m_stopBtn);
    toolbar->addWidget(m_pauseBtn);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("Ready | Hotkey: F6 to start/stop");
}

void MainWindow::onStartClicked()
{
    m_isRunning = true;
    m_isPaused = false;
    m_statusLabel->setText("Running...");
    m_statusLabel->setStyleSheet("font-size: 14px; padding: 10px; color: green;");
    statusBar()->showMessage("Running | Clicks: 0 | Press F6 to stop");
}

void MainWindow::onStopClicked()
{
    m_isRunning = false;
    m_isPaused = false;
    m_statusLabel->setText("Stopped");
    m_statusLabel->setStyleSheet("font-size: 14px; padding: 10px; color: red;");
    statusBar()->showMessage("Stopped | Press F6 to start");
}

void MainWindow::onPauseClicked()
{
    if (m_isRunning) {
        m_isPaused = !m_isPaused;
        if (m_isPaused) {
            m_statusLabel->setText("Paused");
            m_statusLabel->setStyleSheet("font-size: 14px; padding: 10px; color: orange;");
            statusBar()->showMessage("Paused | Press F7 to resume");
        } else {
            m_statusLabel->setText("Running...");
            m_statusLabel->setStyleSheet("font-size: 14px; padding: 10px; color: green;");
            statusBar()->showMessage("Running | Clicks: 0 | Press F6 to stop");
        }
    }
}