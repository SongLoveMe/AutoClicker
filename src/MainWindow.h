#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QStatusBar>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onStartClicked();
    void onStopClicked();
    void onPauseClicked();

private:
    void setupUI();
    void setupToolbar();
    void setupStatusBar();

    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    QPushButton* m_pauseBtn;
    QLabel* m_statusLabel;
    bool m_isRunning;
    bool m_isPaused;
};

#endif // MAINWINDOW_H