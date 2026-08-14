#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include <memory>
#include "core/ClickEngine.h"
#include "core/Workflow.h"

class QComboBox;
class QLabel;
class QTableWidget;
class QPushButton;
class QSpinBox;
class QTreeWidget;
class QTreeWidgetItem;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
protected:
    void closeEvent(QCloseEvent* event) override;
private slots:
    void newWorkflow(); void openWorkflow(); void saveWorkflow(); void saveWorkflowAs(); void importWorkflow(); void exportWorkflow(); void deleteWorkflow(); void renameWorkflow();
    void addSequence(); void renameSequence(); void deleteSequence(); void moveSequence(int delta);
    void selectTreeItem(QTreeWidgetItem* item, int column);
    void onAddPoint(); void onRecordToggled(bool enabled); void onRecorded(const RecordedClick& click);
    void deletePoint(); void deleteAllPoints(); void movePoint(int delta); void editPoint(); void unbindCurrentSequencePoints();
    void configureTransition();
    void startWorkflow(); void stopWorkflow(); void pauseWorkflow();
    void markDirty();
private:
    void buildUi(); void setupHotkeys(); void rebuildTree(); void loadSequenceEditor(); void refreshPoints();
    bool confirmUnsaved(); bool hasCurrentSequence() const; WorkflowSequence& currentSequence(); const WorkflowSequence& currentSequence() const;
    void synchronizeWorkflowTargetWindow();
    QString defaultWorkflowsDir() const; void setCurrentPath(const QString& path); void updateTitle();
    void applyTheme(); void updateRunStatus(const QString& text, const QString& color = QStringLiteral("#475569"));
    void updateRunControls();
    bool resolveWorkflowWindow(uintptr_t* id) const;
    bool resolveSequenceWindow(const WorkflowSequence& sequence, uintptr_t* id) const;

    std::shared_ptr<PlatformAdapter> m_platform;
    std::shared_ptr<ClickEngine> m_engine;
    Workflow m_workflow;
    QString m_path;
    bool m_dirty = false;
    int m_currentSequenceIndex = -1;
    int m_startSequenceIndex = 0;
    bool m_recording = false;
    bool m_refreshingPoints = false;
    bool m_pointsEditing = false;
    QDateTime m_lastRecordTime;

    QTreeWidget* m_tree = nullptr;
    QTableWidget* m_points = nullptr;
    QSpinBox* m_loopCount = nullptr;
    QComboBox* m_startSequenceCombo = nullptr;
    QComboBox* m_clickMethodCombo = nullptr;
    QLabel* m_windowLabel = nullptr;
    QLabel* m_conditionLabel = nullptr;
    QLabel* m_runLabel = nullptr;
    QLabel* m_detailLabel = nullptr;
    QPushButton* m_recordButton = nullptr;
    QPushButton* m_startButton = nullptr;
    QPushButton* m_pauseButton = nullptr;
    QPushButton* m_stopButton = nullptr;
};

#endif
