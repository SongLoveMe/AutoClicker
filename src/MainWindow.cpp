#include "MainWindow.h"
#include "core/WindowsPlatformAdapter.h"
#include "core/WorkflowStore.h"
#include "core/SequencePointGeometry.h"
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QBuffer>
#include <QDir>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>
#include <utility>

namespace {
QPushButton* makeButton(const QString& text, QWidget* parent, const char* objectName = "") {
    auto* result = new QPushButton(text, parent);
    result->setObjectName(objectName);
    return result;
}
QString clickPointText(const SequencePoint& point) {
    const QString mouse = point.button == RecordedButton::Left ? QStringLiteral("左键")
        : point.button == RecordedButton::Right ? QStringLiteral("右键") : QStringLiteral("中键");
    const QString boundTo = point.hasWindowBinding
        ? (QString::fromStdWString(point.windowTitle).isEmpty() ? QString::fromStdWString(point.windowClass) : QString::fromStdWString(point.windowTitle))
        : QStringLiteral("绝对坐标");
    return QStringLiteral("%1  (%2, %3)  ·  %4 ms  ·  %5").arg(mouse).arg(point.screenX).arg(point.screenY).arg(point.intervalMs > 0 ? point.intervalMs : 100).arg(boundTo);
}

class MouseButtonDelegate final : public QStyledItemDelegate {
public:
    explicit MouseButtonDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const override {
        auto* editor = new QComboBox(parent);
        editor->addItems({QStringLiteral("左键"), QStringLiteral("右键"), QStringLiteral("中键")});
        return editor;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        auto* combo = qobject_cast<QComboBox*>(editor);
        if (!combo) return;
        combo->setCurrentText(index.data(Qt::DisplayRole).toString());
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        auto* combo = qobject_cast<QComboBox*>(editor);
        if (combo) model->setData(index, combo->currentText(), Qt::EditRole);
    }
};

}

namespace {
class RegionCanvas final : public QWidget {
public:
    enum class Mode { Polygon, Rectangle };
    RegionCanvas(const QImage& image, Mode mode, QWidget* parent = nullptr)
        : QWidget(parent), m_image(image), m_mode(mode) { setFixedSize(image.size()); setCursor(Qt::CrossCursor); }
    QVector<QPointF> polygon() const {
        if (m_mode == Mode::Polygon) return m_points;
        const QRect rect = QRect(m_dragStart, m_dragEnd).normalized();
        if (rect.width() < 2 || rect.height() < 2) return {};
        return {rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft()};
    }
    QImage croppedImage() const { return m_image.copy(QRect(m_dragStart, m_dragEnd).normalized()); }
    QColor sampledColor() const { return m_sampledColor; }
protected:
    void mousePressEvent(QMouseEvent* event) override {
        const QPoint point = event->position().toPoint();
        if (event->button() == Qt::RightButton && m_mode == Mode::Polygon && rect().contains(point)) { m_sampledColor = m_image.pixelColor(point); update(); return; }
        if (event->button() != Qt::LeftButton) return;
        if (m_mode == Mode::Rectangle) { m_dragStart = point; m_dragEnd = point; m_dragging = true; }
        else { m_points.append(point); }
        update();
    }
    void mouseMoveEvent(QMouseEvent* event) override { if (m_mode == Mode::Rectangle && m_dragging) { m_dragEnd = event->position().toPoint(); update(); } }
    void mouseReleaseEvent(QMouseEvent* event) override { if (m_mode == Mode::Rectangle && event->button() == Qt::LeftButton) { m_dragEnd = event->position().toPoint(); m_dragging = false; update(); } }
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this); painter.drawImage(0, 0, m_image); painter.setRenderHint(QPainter::Antialiasing);
        QPen pen(QColor(37, 99, 235), 2); painter.setPen(pen); painter.setBrush(QColor(37, 99, 235, 45));
        if (m_mode == Mode::Rectangle) { const QRect selection(m_dragStart, m_dragEnd); if (!selection.isNull()) painter.drawRect(selection.normalized()); }
        else if (!m_points.isEmpty()) { painter.drawPolyline(QPolygonF(m_points)); for (const QPointF& p : m_points) painter.drawEllipse(p, 4, 4); if (m_points.size() > 2) painter.drawLine(m_points.last(), m_points.first()); }
        if (m_sampledColor.isValid()) { painter.setBrush(m_sampledColor); painter.setPen(Qt::white); painter.drawEllipse(QPoint(14, 14), 9, 9); }
    }
private:
    QImage m_image; Mode m_mode; QVector<QPointF> m_points; QPoint m_dragStart; QPoint m_dragEnd; bool m_dragging = false; QColor m_sampledColor;
};

class RegionDialog final : public QDialog {
public:
    RegionDialog(const QImage& image, RegionCanvas::Mode mode, QWidget* parent)
        : QDialog(parent), m_canvas(new RegionCanvas(image, mode, this)), m_mode(mode) {
        setWindowTitle(mode == RegionCanvas::Mode::Polygon ? QStringLiteral("绘制多边形检测区域") : QStringLiteral("截取模板图片")); resize(780, 560);
        auto* layout = new QVBoxLayout(this); auto* hint = new QLabel(mode == RegionCanvas::Mode::Polygon ? QStringLiteral("左键依次添加顶点；右键在区域内吸取颜色；完成后点击确定。") : QStringLiteral("在截图上按住左键拖拽出模板范围，然后点击确定。"), this);
        hint->setWordWrap(true); layout->addWidget(hint); auto* scroll = new QScrollArea(this); scroll->setWidget(m_canvas); scroll->setWidgetResizable(false); layout->addWidget(scroll, 1);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this); layout->addWidget(buttons); connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
    QVector<QPointF> polygon() const { return m_canvas->polygon(); }
    QColor color() const { return m_canvas->sampledColor(); }
    QImage crop() const { return m_canvas->croppedImage(); }
    bool valid() const { return m_mode == RegionCanvas::Mode::Polygon ? polygon().size() >= 3 : !crop().isNull(); }
private:
    RegionCanvas* m_canvas; RegionCanvas::Mode m_mode;
};

class ClickableImageLabel final : public QLabel {
public:
    explicit ClickableImageLabel(QWidget* parent = nullptr) : QLabel(parent) {
        setAlignment(Qt::AlignCenter);
        setFrameShape(QFrame::StyledPanel);
        setCursor(Qt::ArrowCursor);
    }

    void setImage(const QImage& image) {
        m_image = image;
        if (m_image.isNull()) {
            clear();
            setCursor(Qt::ArrowCursor);
            setToolTip(QString());
            return;
        }
        setCursor(Qt::PointingHandCursor);
        setToolTip(QStringLiteral("点击预览图片"));
        updatePixmap();
    }

    const QImage& image() const { return m_image; }
    void setClickHandler(std::function<void()> handler) { m_clickHandler = std::move(handler); }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && !m_image.isNull() && m_clickHandler) m_clickHandler();
        QLabel::mousePressEvent(event);
    }

    void resizeEvent(QResizeEvent* event) override {
        QLabel::resizeEvent(event);
        updatePixmap();
    }

private:
    void updatePixmap() {
        if (m_image.isNull() || width() <= 4 || height() <= 4) return;
        setPixmap(QPixmap::fromImage(m_image).scaled(size() - QSize(8, 8), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    QImage m_image;
    std::function<void()> m_clickHandler;
};

class ImagePreviewDialog final : public QDialog {
public:
    explicit ImagePreviewDialog(const QImage& image, QWidget* parent = nullptr)
        : QDialog(parent), m_image(image), m_imageLabel(new QLabel(this)), m_scrollArea(new QScrollArea(this)) {
        setWindowTitle(QStringLiteral("模板预览"));
        setWindowFlag(Qt::Tool, true);
        resize(720, 520);
        auto* layout = new QVBoxLayout(this);
        m_imageLabel->setAlignment(Qt::AlignCenter);
        m_scrollArea->setWidget(m_imageLabel);
        m_scrollArea->setWidgetResizable(true);
        m_scrollArea->setAlignment(Qt::AlignCenter);
        layout->addWidget(m_scrollArea, 1);
        QTimer::singleShot(0, this, [this]() { render(); });
    }
private:
    void render() {
        if (m_image.isNull()) return;
        const QSize available = m_scrollArea->viewport()->size() - QSize(8, 8);
        if (available.width() <= 0 || available.height() <= 0) return;
        m_imageLabel->setPixmap(QPixmap::fromImage(m_image).scaled(available, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
protected:
    void resizeEvent(QResizeEvent* event) override {
        QDialog::resizeEvent(event);
        render();
    }
    QImage m_image;
    QLabel* m_imageLabel;
    QScrollArea* m_scrollArea;
};
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_platform(std::make_shared<WindowsPlatformAdapter>())
    , m_engine(std::make_shared<ClickEngine>(m_platform))
{
    m_workflow.name = QStringLiteral("新工作流");
    m_workflow.sequences.append(WorkflowSequence{QStringLiteral("序列 1")});
    m_currentSequenceIndex = 0;
    buildUi();
    applyTheme();
    QTimer::singleShot(100, this, &MainWindow::setupHotkeys);
    rebuildTree();
    loadSequenceEditor();
    auto* pointViewTimer = new QTimer(this);
    pointViewTimer->setInterval(500);
    connect(pointViewTimer, &QTimer::timeout, this, [this]() {
        if (isVisible() && hasCurrentSequence() && !m_refreshingPoints && !m_pointsEditing && !m_points->hasFocus()) refreshPoints();
    });
    pointViewTimer->start();

    connect(m_engine.get(), &ClickEngine::clickPerformed, this, [this](int, int, uintptr_t) {
        m_detailLabel->setText(QStringLiteral("总点击：%1").arg(m_engine->getTotalClicks()));
    });
    connect(m_engine.get(), &ClickEngine::workflowSequenceChanged, this, [this](const QString& name, int, int loop) {
        updateRunStatus(QStringLiteral("正在执行：%1 · 第 %2 轮").arg(name).arg(loop), QStringLiteral("#047857"));
    });
    connect(m_engine.get(), &ClickEngine::visualConditionChecked, this, [this](const QString& detail, bool, int hits) {
        m_detailLabel->setText(detail + QStringLiteral(" · 本轮%1").arg(hits ? QStringLiteral("命中") : QStringLiteral("未命中")));
    });
    connect(m_engine.get(), &ClickEngine::workflowPaused, this, [this](const QString& reason) {
        updateRunStatus(QStringLiteral("已暂停：") + reason, QStringLiteral("#b91c1c"));
        updateRunControls();
    });
    connect(m_engine.get(), &ClickEngine::workflowFinished, this, [this](const QString& reason) {
        updateRunStatus(QStringLiteral("已完成：") + reason);
        updateRunControls();
    });
    connect(m_engine.get(), &ClickEngine::workflowCompleted, this, [this](const QString& reason) {
        QMessageBox::information(this, QStringLiteral("工作流已完成"), QStringLiteral("工作流已完全结束。\n%1").arg(reason));
    });
    connect(m_engine.get(), &ClickEngine::finished, this, [this]() {
        updateRunControls();
    });
    connect(m_engine.get(), &ClickEngine::errorOccurred, this, [this](const QString& reason) {
        updateRunStatus(QStringLiteral("无法开始：") + reason, QStringLiteral("#b91c1c"));
        updateRunControls();
    });
}

MainWindow::~MainWindow()
{
    m_platform->unregisterHotkey(1);
    m_platform->unregisterHotkey(2);
    m_platform->unregisterHotkey(3);
    if (m_recording) m_platform->stopMouseHook();
}

void MainWindow::setupHotkeys()
{
    m_platform->registerHotkey(1, Qt::Key_F6, 0, [this]() {
        QMetaObject::invokeMethod(this, [this]() { m_engine->isRunning() ? stopWorkflow() : startWorkflow(); }, Qt::QueuedConnection);
    });
    m_platform->registerHotkey(2, Qt::Key_F7, 0, [this]() {
        QMetaObject::invokeMethod(this, [this]() { pauseWorkflow(); }, Qt::QueuedConnection);
    });
    m_platform->registerHotkey(3, Qt::Key_F8, 0, [this]() {
        QMetaObject::invokeMethod(this, [this]() {
            if (m_engine->isRunning()) { statusBar()->showMessage(QStringLiteral("工作流运行中，无法录制"), 3000); return; }
            onRecordToggled(!m_recording);
        }, Qt::QueuedConnection);
    });
}

void MainWindow::buildUi()
{
    setMinimumSize(640, 420);
    resize(900, 560);
    auto* toolbar = new QToolBar(QStringLiteral("工作流"), this);
    toolbar->setMovable(false);
    auto* fileMenu = new QMenu(this);
    QAction* open = fileMenu->addAction(QStringLiteral("打开…"));
    QAction* save = fileMenu->addAction(QStringLiteral("保存"));
    QAction* saveAs = fileMenu->addAction(QStringLiteral("另存为…"));
    fileMenu->addSeparator();
    QAction* import = fileMenu->addAction(QStringLiteral("导入…"));
    QAction* exportButton = fileMenu->addAction(QStringLiteral("导出…"));
    fileMenu->addSeparator();
    QAction* removeWorkflow = fileMenu->addAction(QStringLiteral("删除工作流"));
    auto* fileButton = new QToolButton(toolbar); fileButton->setText(QStringLiteral("文件")); fileButton->setMenu(fileMenu); fileButton->setPopupMode(QToolButton::InstantPopup);
    toolbar->addWidget(fileButton); toolbar->addSeparator();
    toolbar->addWidget(new QLabel(QStringLiteral("起始序列"), this)); m_startSequenceCombo = new QComboBox(this); toolbar->addWidget(m_startSequenceCombo); toolbar->addSeparator();
    m_startButton = makeButton(QStringLiteral("开始"), this, "primaryButton");
    m_pauseButton = makeButton(QStringLiteral("暂停"), this);
    m_stopButton = makeButton(QStringLiteral("停止"), this, "dangerButton");
    m_pauseButton->setEnabled(false);
    m_stopButton->setEnabled(false);
    toolbar->addWidget(m_startButton); toolbar->addWidget(m_pauseButton); toolbar->addWidget(m_stopButton);
    connect(open, &QAction::triggered, this, &MainWindow::openWorkflow);
    connect(save, &QAction::triggered, this, &MainWindow::saveWorkflow);
    connect(saveAs, &QAction::triggered, this, &MainWindow::saveWorkflowAs);
    connect(import, &QAction::triggered, this, &MainWindow::importWorkflow);
    connect(exportButton, &QAction::triggered, this, &MainWindow::exportWorkflow);
    connect(removeWorkflow, &QAction::triggered, this, &MainWindow::deleteWorkflow);
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::startWorkflow);
    connect(m_pauseButton, &QPushButton::clicked, this, &MainWindow::pauseWorkflow);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopWorkflow);
    connect(m_startSequenceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index >= 0) m_startSequenceIndex = m_startSequenceCombo->itemData(index).toInt();
    });

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(true);
    auto* left = new QWidget(splitter); left->setMinimumWidth(120); auto* leftLayout = new QVBoxLayout(left); leftLayout->setContentsMargins(4, 4, 2, 4); leftLayout->setSpacing(4);
    auto* workflowTitle = new QLabel(QStringLiteral("工作流"), left); workflowTitle->setToolTip(QStringLiteral("右键工作流或序列进行编辑"));
    leftLayout->addWidget(workflowTitle);
    m_tree = new QTreeWidget(left); m_tree->setHeaderHidden(true); leftLayout->addWidget(m_tree, 1);
    connect(m_tree, &QTreeWidget::itemClicked, this, &MainWindow::selectTreeItem);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (auto* item = m_tree->itemAt(pos)) selectTreeItem(item, 0);
        QMenu menu(this);
        menu.addAction(QStringLiteral("新建工作流"), this, &MainWindow::newWorkflow);
        if (m_tree->itemAt(pos) && m_tree->itemAt(pos)->parent() == nullptr) menu.addAction(QStringLiteral("重命名工作流"), this, &MainWindow::renameWorkflow);
        menu.addAction(QStringLiteral("添加序列"), this, &MainWindow::addSequence);
        if (hasCurrentSequence()) {
            menu.addSeparator();
            menu.addAction(QStringLiteral("重命名序列"), this, &MainWindow::renameSequence);
            menu.addAction(QStringLiteral("上移序列"), this, [this]() { moveSequence(-1); });
            menu.addAction(QStringLiteral("下移序列"), this, [this]() { moveSequence(1); });
            menu.addAction(QStringLiteral("删除序列"), this, &MainWindow::deleteSequence);
        }
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    });

    auto* middle = new QWidget(splitter); middle->setMinimumWidth(200); auto* middleLayout = new QVBoxLayout(middle); middleLayout->setContentsMargins(2, 4, 2, 4); middleLayout->setSpacing(4);
    auto* sequenceGroup = new QGroupBox(QStringLiteral("当前序列"), middle); auto* sequenceForm = new QFormLayout(sequenceGroup);
    m_loopCount = new QSpinBox(sequenceGroup); m_loopCount->setRange(-1, 1000000); m_loopCount->setSpecialValueText(QStringLiteral("无限")); sequenceForm->addRow(QStringLiteral("循环轮数"), m_loopCount); middleLayout->addWidget(sequenceGroup);
    auto* pointsTitle = new QLabel(QStringLiteral("点击点"), middle); pointsTitle->setToolTip(QStringLiteral("右键可添加当前位置、编辑或删除点位"));
    middleLayout->addWidget(pointsTitle);
    m_points = new QTableWidget(middle); m_points->setColumnCount(7); m_points->setHorizontalHeaderLabels({QStringLiteral("全局坐标"), QStringLiteral("当前相对坐标"), QStringLiteral("绑定窗口"), QStringLiteral("点击次数"), QStringLiteral("点击间隔"), QStringLiteral("下一点间隔"), QStringLiteral("按钮")}); m_points->setSelectionBehavior(QAbstractItemView::SelectRows); m_points->setSelectionMode(QAbstractItemView::SingleSelection); m_points->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed); auto* buttonDelegate = new MouseButtonDelegate(m_points); m_points->setItemDelegateForColumn(6, buttonDelegate); m_points->setAlternatingRowColors(true); m_points->setWordWrap(false); m_points->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded); m_points->verticalHeader()->setDefaultSectionSize(25); m_points->horizontalHeader()->setStretchLastSection(false); m_points->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); m_points->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents); m_points->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed); m_points->horizontalHeader()->resizeSection(2, 150); m_points->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents); m_points->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents); m_points->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents); middleLayout->addWidget(m_points, 1);
    m_recordButton = makeButton(QStringLiteral("开始录制"), middle, "primaryButton"); middleLayout->addWidget(m_recordButton); m_recordButton->setToolTip(QStringLiteral("录制鼠标点击并自动绑定所在窗口"));
    connect(m_recordButton, &QPushButton::clicked, this, [this]() { onRecordToggled(!m_recording); });
    m_points->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_points, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        const bool onItem = m_points->itemAt(pos) != nullptr;
        if (onItem) m_points->selectRow(m_points->itemAt(pos)->row()); else m_points->clearSelection();
        QMenu menu(this);
        menu.addAction(QStringLiteral("添加当前鼠标位置"), this, &MainWindow::onAddPoint);
        menu.addAction(QStringLiteral("删除当前序列全部点"), this, &MainWindow::deleteAllPoints);
        if (!onItem) menu.addAction(QStringLiteral("取消当前序列全部点位绑定"), this, &MainWindow::unbindCurrentSequencePoints);
        if (onItem && m_points->currentRow() >= 0) {
            menu.addSeparator();
            const int row = m_points->currentRow();
            auto* moveUp = menu.addAction(QStringLiteral("上移点"), this, [this]() { movePoint(-1); });
            auto* moveDown = menu.addAction(QStringLiteral("下移点"), this, [this]() { movePoint(1); });
            moveUp->setEnabled(row > 0);
            moveDown->setEnabled(hasCurrentSequence() && row + 1 < currentSequence().points.size());
            menu.addAction(QStringLiteral("编辑点击点"), this, &MainWindow::editPoint);
            menu.addAction(QStringLiteral("删除点击点"), this, &MainWindow::deletePoint);
        }
        menu.exec(m_points->viewport()->mapToGlobal(pos));
    });
    connect(m_points->itemDelegate(), &QAbstractItemDelegate::closeEditor, this, [this](QWidget*, QAbstractItemDelegate::EndEditHint) {
        m_pointsEditing = false;
        if (!m_refreshingPoints) refreshPoints();
    });
    connect(buttonDelegate, &QAbstractItemDelegate::closeEditor, this, [this](QWidget*, QAbstractItemDelegate::EndEditHint) {
        m_pointsEditing = false;
        if (!m_refreshingPoints) refreshPoints();
    });
    connect(m_points, &QTableWidget::cellChanged, this, [this](int row, int column) {
        if (m_refreshingPoints || !hasCurrentSequence() || row < 0 || row >= currentSequence().points.size()) return;
        SequencePoint& point = currentSequence().points[row];
        const QString value = m_points->item(row, column) ? m_points->item(row, column)->text().trimmed() : QString();
        bool ok = false;
        if (column == 0 || column == 1) {
            const QStringList parts = value.split(QRegularExpression(QStringLiteral("[,，\\s]+")), Qt::SkipEmptyParts);
            if (parts.size() == 2) { bool firstOk = false, secondOk = false; const int first = parts[0].toInt(&firstOk); const int second = parts[1].toInt(&secondOk); ok = firstOk && secondOk; if (ok) { const bool bound = pointHasWindowBinding(point); const uintptr_t id = bound ? m_platform->findWindowByLocator(point.windowTitle, point.processName, point.windowClass) : 0; WindowInfo info{}; if (id) info = m_platform->isWindowMinimized(id) ? m_platform->getWindowPlacementInfo(id) : m_platform->getWindowInfo(id); if (column == 0) { point.screenX = first; point.screenY = second; if (id && info.width > 0 && info.height > 0) { const double sx = point.boundWindowWidth > 0 ? double(point.boundWindowWidth) / info.width : 1.0; const double sy = point.boundWindowHeight > 0 ? double(point.boundWindowHeight) / info.height : 1.0; point.relX = qRound((point.screenX - info.x) * sx); point.relY = qRound((point.screenY - info.y) * sy); } } else { point.relX = first; point.relY = second; if (id) { const ResolvedPointGeometry resolved = resolveSequencePoint(point, &info); point.screenX = resolved.globalX; point.screenY = resolved.globalY; } } } }
        } else if (column == 3) { const int number = value.toInt(&ok); if (ok && number >= 1) point.clickCount = number; }
        else if (column == 4) { const int number = value.toInt(&ok); if (ok && number >= 1) point.clickIntervalMs = number; }
        else if (column == 5) { const int number = value.toInt(&ok); if (ok && number >= 1) point.intervalMs = number; }
        else if (column == 6) {
            if (value == QStringLiteral("左键")) { point.button = RecordedButton::Left; ok = true; }
            else if (value == QStringLiteral("右键")) { point.button = RecordedButton::Right; ok = true; }
            else if (value == QStringLiteral("中键")) { point.button = RecordedButton::Middle; ok = true; }
        }
        else return;
        if (!ok) { statusBar()->showMessage(QStringLiteral("点位数据格式无效"), 2500); return; }
        markDirty();
    });
    connect(m_points, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        if (column == 2) { m_points->selectRow(row); editPoint(); }
        else m_pointsEditing = true;
    });
    connect(m_loopCount, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) { if (hasCurrentSequence()) { currentSequence().loopCount = value; markDirty(); } });

    auto* right = new QWidget(splitter); right->setMinimumWidth(200); auto* rightLayout = new QVBoxLayout(right); rightLayout->setContentsMargins(2, 4, 4, 4); rightLayout->setSpacing(4);
    auto* windowGroup = new QGroupBox(QStringLiteral("目标窗口"), right); auto* windowLayout = new QVBoxLayout(windowGroup);
    m_windowLabel = new QLabel(QStringLiteral("由序列点自动绑定"), windowGroup);
    m_clickMethodCombo = new QComboBox(windowGroup); m_clickMethodCombo->addItem(QStringLiteral("模拟鼠标（需前台窗口）"), int(ClickMethod::SimulateMouse)); m_clickMethodCombo->addItem(QStringLiteral("无干扰点击（支持后台/最小化）"), int(ClickMethod::NoInterference)); m_clickMethodCombo->setToolTip(QStringLiteral("无干扰后台模式不支持图片或颜色视觉条件"));
    windowLayout->addWidget(m_windowLabel); windowLayout->addWidget(new QLabel(QStringLiteral("点击方式"), windowGroup)); windowLayout->addWidget(m_clickMethodCombo); rightLayout->addWidget(windowGroup);
    auto* conditionGroup = new QGroupBox(QStringLiteral("跳转条件"), right); auto* conditionLayout = new QVBoxLayout(conditionGroup);
    m_conditionLabel = new QLabel(QStringLiteral("未配置：序列完成后将停止"), conditionGroup); m_conditionLabel->setWordWrap(true); auto* configure = makeButton(QStringLiteral("配置条件"), conditionGroup, "primaryButton"); conditionLayout->addWidget(m_conditionLabel); conditionLayout->addWidget(configure); rightLayout->addWidget(conditionGroup);
    auto* stateGroup = new QGroupBox(QStringLiteral("运行状态"), right); auto* stateLayout = new QVBoxLayout(stateGroup); m_runLabel = new QLabel(QStringLiteral("就绪"), stateGroup); m_detailLabel = new QLabel(QStringLiteral("选择工作流和序列开始编辑"), stateGroup); m_detailLabel->setWordWrap(true); stateLayout->addWidget(m_runLabel); stateLayout->addWidget(m_detailLabel); rightLayout->addWidget(stateGroup); rightLayout->addStretch();
    connect(m_clickMethodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 0) return;
        m_workflow.clickMethod = static_cast<ClickMethod>(m_clickMethodCombo->itemData(index).toInt());
        markDirty();
    });
    connect(configure, &QPushButton::clicked, this, &MainWindow::configureTransition);
    splitter->addWidget(left); splitter->addWidget(middle); splitter->addWidget(right); splitter->setStretchFactor(0, 1); splitter->setStretchFactor(1, 3); splitter->setStretchFactor(2, 2); splitter->setSizes({150, 400, 250});
    auto* root = new QWidget(this); auto* rootLayout = new QVBoxLayout(root); rootLayout->setContentsMargins(0, 0, 0, 0); rootLayout->setSpacing(0); rootLayout->addWidget(toolbar); rootLayout->addWidget(splitter, 1); setCentralWidget(root);
    statusBar()->showMessage(QStringLiteral("右键编辑 · F6 开始/停止 · F7 暂停/继续 · F8 录制"));
}

void MainWindow::applyTheme()
{
    qApp->setStyleSheet(QStringLiteral("QWidget { font-family: 'Microsoft YaHei'; color: #172033; } QMainWindow { background: #f5f7fb; } QToolBar { background: #fff; border: 0; border-bottom: 1px solid #dce3ee; padding: 2px 4px; spacing: 3px; } QGroupBox { background: #fff; border: 1px solid #dce3ee; border-radius: 5px; margin-top: 8px; padding: 5px; font-weight: 600; } QGroupBox::title { left: 6px; padding: 0 2px; } QPushButton, QToolButton { background: #fff; border: 1px solid #cbd5e1; border-radius: 4px; padding: 4px 7px; min-height: 22px; } QPushButton:hover, QToolButton:hover { background: #eef4ff; border-color: #4f83e8; } QPushButton#primaryButton { background: #2563eb; color: white; border: 0; } QPushButton#primaryButton:hover { background: #1d4ed8; } QPushButton#dangerButton { color: #b91c1c; } QTreeWidget, QTableWidget, QComboBox, QSpinBox { background: #fff; border: 1px solid #cbd5e1; border-radius: 4px; padding: 2px 4px; } QTreeWidget::item:selected, QTableWidget::item:selected { background: #dbeafe; color: #172033; } QLabel { color: #475569; }"));
}

bool MainWindow::hasCurrentSequence() const { return m_currentSequenceIndex >= 0 && m_currentSequenceIndex < m_workflow.sequences.size(); }
WorkflowSequence& MainWindow::currentSequence() { return m_workflow.sequences[m_currentSequenceIndex]; }
const WorkflowSequence& MainWindow::currentSequence() const { return m_workflow.sequences[m_currentSequenceIndex]; }
QString MainWindow::defaultWorkflowsDir() const { return QCoreApplication::applicationDirPath() + QStringLiteral("/workflows"); }
void MainWindow::setCurrentPath(const QString& path) { m_path = path; m_dirty = false; rebuildTree(); }
void MainWindow::updateTitle() { setWindowTitle(QStringLiteral("AutoClicker v2.4")); }
void MainWindow::updateRunStatus(const QString& text, const QString& color) { m_runLabel->setText(text); m_runLabel->setStyleSheet(QStringLiteral("font-weight: 700; color: %1;").arg(color)); }
void MainWindow::updateRunControls()
{
    const bool running = m_engine->isRunning();
    m_startButton->setEnabled(!running);
    m_pauseButton->setEnabled(running);
    m_stopButton->setEnabled(running);
    m_pauseButton->setText(m_engine->isPaused() ? QStringLiteral("继续") : QStringLiteral("暂停"));
}

void MainWindow::rebuildTree()
{
    m_tree->clear();
    auto* root = new QTreeWidgetItem(m_tree, QStringList() << m_workflow.name + (m_dirty ? QStringLiteral(" *") : QString())); root->setData(0, Qt::UserRole, -1); root->setExpanded(true);
    for (int i = 0; i < m_workflow.sequences.size(); ++i) { auto* child = new QTreeWidgetItem(root, QStringList() << m_workflow.sequences[i].name); child->setData(0, Qt::UserRole, i); }
    if (m_currentSequenceIndex >= 0 && m_currentSequenceIndex < root->childCount()) m_tree->setCurrentItem(root->child(m_currentSequenceIndex));
    m_startSequenceIndex = qBound(0, m_startSequenceIndex, qMax(0, m_workflow.sequences.size() - 1));
    m_startSequenceCombo->blockSignals(true); m_startSequenceCombo->clear(); for (int i = 0; i < m_workflow.sequences.size(); ++i) m_startSequenceCombo->addItem(m_workflow.sequences[i].name, i); m_startSequenceCombo->setCurrentIndex(m_startSequenceCombo->findData(m_startSequenceIndex)); m_startSequenceCombo->blockSignals(false);
    updateTitle();
}

void MainWindow::refreshPoints() {
    m_refreshingPoints = true;
    const int selectedRow = m_points->currentRow();
    const int horizontalScroll = m_points->horizontalScrollBar()->value();
    const int verticalScroll = m_points->verticalScrollBar()->value();
    m_points->setRowCount(0);
    if (!hasCurrentSequence()) { m_refreshingPoints = false; return; }
    for (int row = 0; row < currentSequence().points.size(); ++row) {
        const SequencePoint& point = currentSequence().points[row];
        m_points->insertRow(row);
        WindowInfo currentWindow{};
        const bool bound = pointHasWindowBinding(point);
        const uintptr_t currentId = bound ? m_platform->findWindowByLocator(point.windowTitle, point.processName, point.windowClass) : 0;
        if (currentId) currentWindow = m_platform->isWindowMinimized(currentId) ? m_platform->getWindowPlacementInfo(currentId) : m_platform->getWindowInfo(currentId);
        const ResolvedPointGeometry resolved = resolveSequencePoint(point, currentId ? &currentWindow : nullptr);
        const QString binding = !bound ? QStringLiteral("不绑定窗口") : (currentId ? (QString::fromStdWString(point.windowTitle).isEmpty() ? QString::fromStdWString(point.windowClass) : QString::fromStdWString(point.windowTitle)) : QStringLiteral("窗口未定位"));
        const QString button = point.button == RecordedButton::Left ? QStringLiteral("左键") : point.button == RecordedButton::Right ? QStringLiteral("右键") : QStringLiteral("中键");
        const QStringList values{QStringLiteral("%1, %2").arg(resolved.globalX).arg(resolved.globalY), bound ? QStringLiteral("%1, %2").arg(resolved.currentRelX).arg(resolved.currentRelY) : QStringLiteral("-"), binding, QString::number(qMax(1, point.clickCount)), QString::number(qMax(1, point.clickIntervalMs)), QString::number(qMax(1, point.intervalMs)), button};
        for (int column = 0; column < values.size(); ++column) { auto* item = new QTableWidgetItem(values[column]); if (column == 2) item->setFlags(item->flags() & ~Qt::ItemIsEditable); m_points->setItem(row, column, item); }
        m_points->item(row, 2)->setToolTip(binding);
    }
    if (selectedRow >= 0 && selectedRow < m_points->rowCount()) m_points->selectRow(selectedRow);
    m_points->horizontalScrollBar()->setValue(horizontalScroll);
    m_points->verticalScrollBar()->setValue(verticalScroll);
    m_refreshingPoints = false;
}
void MainWindow::loadSequenceEditor()
{
    if (!hasCurrentSequence()) return;
    synchronizeWorkflowTargetWindow();
    m_loopCount->blockSignals(true); m_loopCount->setValue(currentSequence().loopCount); m_loopCount->blockSignals(false); refreshPoints();
    m_clickMethodCombo->blockSignals(true); m_clickMethodCombo->setCurrentIndex(m_clickMethodCombo->findData(int(m_workflow.clickMethod))); m_clickMethodCombo->blockSignals(false);
    const VisualTransition& transition = currentSequence().transition;
    if (!transition.enabled) m_conditionLabel->setText(QStringLiteral("未配置：达到循环轮数后停止"));
    else {
        const QString type = transition.type == VisualConditionType::Template ? QStringLiteral("图片出现")
            : transition.type == VisualConditionType::ColorRatio ? QStringLiteral("颜色占比") : QStringLiteral("循环结束自动跳转");
        m_conditionLabel->setText(QStringLiteral("%1 → %2").arg(type).arg(m_workflow.sequences.value(transition.targetSequenceIndex).name));
    }
}
void MainWindow::selectTreeItem(QTreeWidgetItem* item, int) { if (!item) return; const int index = item->data(0, Qt::UserRole).toInt(); if (index >= 0) { m_currentSequenceIndex = index; loadSequenceEditor(); } }
void MainWindow::markDirty() { m_dirty = true; rebuildTree(); }

bool MainWindow::confirmUnsaved()
{
    if (!m_dirty) return true;
    const auto choice = QMessageBox::warning(this, QStringLiteral("未保存修改"), QStringLiteral("当前工作流有未保存修改。"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (choice == QMessageBox::Save) { saveWorkflow(); return !m_dirty; }
    return choice == QMessageBox::Discard;
}
void MainWindow::closeEvent(QCloseEvent* event) { if (confirmUnsaved()) event->accept(); else event->ignore(); }
void MainWindow::newWorkflow()
{
    if (!confirmUnsaved()) return;
    bool ok = false; const QString name = QInputDialog::getText(this, QStringLiteral("新建工作流"), QStringLiteral("名称"), QLineEdit::Normal, QStringLiteral("新工作流"), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    m_workflow = Workflow{}; m_workflow.name = name.trimmed(); m_workflow.sequences = {WorkflowSequence{QStringLiteral("序列 1")}}; m_path.clear(); m_currentSequenceIndex = 0; m_startSequenceIndex = 0; m_dirty = true; rebuildTree(); loadSequenceEditor();
}
void MainWindow::renameWorkflow()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("重命名工作流"), QStringLiteral("名称"), QLineEdit::Normal, m_workflow.name, &ok);
    if (ok && !name.trimmed().isEmpty()) { m_workflow.name = name.trimmed(); markDirty(); }
}
void MainWindow::openWorkflow()
{
    if (!confirmUnsaved()) return;
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开工作流"), defaultWorkflowsDir(), QStringLiteral("AutoClicker 工作流 (*.acworkflow);;v2.3 配置 (*.json)")); if (path.isEmpty()) return;
    Workflow loaded; QString error; const bool legacy = path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive);
    const bool loadedOk = legacy ? WorkflowStore::loadLegacyConfig(path, &loaded, &error) : WorkflowStore::load(path, &loaded, &error);
    if (!loadedOk) { QMessageBox::critical(this, QStringLiteral("无法打开"), error); return; }
    m_workflow = loaded; m_currentSequenceIndex = 0; m_startSequenceIndex = 0; if (legacy) { m_path.clear(); m_dirty = true; rebuildTree(); } else setCurrentPath(path); loadSequenceEditor();
}
void MainWindow::saveWorkflow()
{
    if (m_path.isEmpty()) { saveWorkflowAs(); return; }
    QString error; if (!WorkflowStore::save(m_workflow, m_path, &error)) { QMessageBox::warning(this, QStringLiteral("保存失败"), error); return; }
    m_dirty = false; rebuildTree(); statusBar()->showMessage(QStringLiteral("工作流已保存"), 3000);
}
void MainWindow::saveWorkflowAs()
{
    QDir().mkpath(defaultWorkflowsDir());
    const QString initial = m_path.isEmpty() ? defaultWorkflowsDir() + QStringLiteral("/") + WorkflowStore::safeFileName(m_workflow.name) + QStringLiteral(".acworkflow") : m_path;
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存工作流"), initial, QStringLiteral("AutoClicker 工作流 (*.acworkflow)")); if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".acworkflow"), Qt::CaseInsensitive)) path += QStringLiteral(".acworkflow");
    setCurrentPath(path); saveWorkflow();
}
void MainWindow::importWorkflow()
{
    if (!confirmUnsaved()) return;
    const QString source = QFileDialog::getOpenFileName(this, QStringLiteral("导入工作流"), QString(), QStringLiteral("AutoClicker 工作流 (*.acworkflow)")); if (source.isEmpty()) return;
    Workflow imported; QString error; if (!WorkflowStore::load(source, &imported, &error)) { QMessageBox::warning(this, QStringLiteral("导入失败"), error); return; }
    bool conflict = false; const QDir directory(defaultWorkflowsDir());
    for (const QFileInfo& entry : directory.entryInfoList({QStringLiteral("*.acworkflow")}, QDir::Files)) { Workflow existing; if (WorkflowStore::load(entry.absoluteFilePath(), &existing) && existing.name == imported.name) { conflict = true; break; } }
    if (conflict) {
        const QString base = imported.name + QStringLiteral("（导入）"); int number = 1; imported.name = base;
        while (true) { bool used = false; for (const QFileInfo& entry : directory.entryInfoList({QStringLiteral("*.acworkflow")}, QDir::Files)) { Workflow existing; if (WorkflowStore::load(entry.absoluteFilePath(), &existing) && existing.name == imported.name) { used = true; break; } } if (!used) break; imported.name = base + QString::number(++number); }
    }
    m_workflow = imported; m_path.clear(); m_currentSequenceIndex = 0; m_startSequenceIndex = 0; m_dirty = true; rebuildTree(); loadSequenceEditor(); saveWorkflowAs();
}
void MainWindow::exportWorkflow()
{
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出工作流"), WorkflowStore::safeFileName(m_workflow.name) + QStringLiteral(".acworkflow"), QStringLiteral("AutoClicker 工作流 (*.acworkflow)")); if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".acworkflow"), Qt::CaseInsensitive)) path += QStringLiteral(".acworkflow"); QString error; if (!WorkflowStore::save(m_workflow, path, &error)) QMessageBox::warning(this, QStringLiteral("导出失败"), error);
}
void MainWindow::deleteWorkflow()
{
    if (m_path.isEmpty()) { QMessageBox::information(this, QStringLiteral("未保存工作流"), QStringLiteral("当前工作流尚未保存为文件，无需删除。")); return; }
    const QString name = m_workflow.name;
    if (QMessageBox::warning(this, QStringLiteral("永久删除工作流"), QStringLiteral("将永久删除“%1”及其中的模板图片，且无法恢复。确认删除？").arg(name), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    if (!QFile::remove(m_path)) { QMessageBox::warning(this, QStringLiteral("删除失败"), QStringLiteral("无法删除工作流文件。")); return; }
    m_workflow = Workflow{}; m_workflow.name = QStringLiteral("新工作流"); m_workflow.sequences = {WorkflowSequence{QStringLiteral("序列 1")}}; m_path.clear(); m_currentSequenceIndex = 0; m_startSequenceIndex = 0; m_dirty = false; rebuildTree(); loadSequenceEditor(); statusBar()->showMessage(QStringLiteral("工作流已永久删除"), 3000);
}

void MainWindow::addSequence()
{
    bool ok = false; const QString defaultName = QStringLiteral("序列 %1").arg(m_workflow.sequences.size() + 1); const QString name = QInputDialog::getText(this, QStringLiteral("添加序列"), QStringLiteral("名称"), QLineEdit::Normal, defaultName, &ok);
    if (!ok || name.trimmed().isEmpty()) return; m_workflow.sequences.append(WorkflowSequence{name.trimmed()}); m_currentSequenceIndex = m_workflow.sequences.size() - 1; markDirty(); loadSequenceEditor();
}
void MainWindow::renameSequence() { if (!hasCurrentSequence()) return; bool ok = false; const QString name = QInputDialog::getText(this, QStringLiteral("重命名序列"), QStringLiteral("名称"), QLineEdit::Normal, currentSequence().name, &ok); if (ok && !name.trimmed().isEmpty()) { currentSequence().name = name.trimmed(); markDirty(); } }
void MainWindow::deleteSequence()
{
    if (!hasCurrentSequence() || m_workflow.sequences.size() == 1) { QMessageBox::information(this, QStringLiteral("无法删除"), QStringLiteral("工作流至少需要一个序列。")); return; }
    if (QMessageBox::question(this, QStringLiteral("永久删除"), QStringLiteral("删除序列“%1”及其点击点？").arg(currentSequence().name)) != QMessageBox::Yes) return;
    const int deletedIndex = m_currentSequenceIndex;
    m_workflow.sequences.removeAt(deletedIndex); m_currentSequenceIndex = qMax(0, deletedIndex - 1);
    if (m_startSequenceIndex > deletedIndex) --m_startSequenceIndex;
    else if (m_startSequenceIndex == deletedIndex) m_startSequenceIndex = qMin(deletedIndex, m_workflow.sequences.size() - 1);
    for (WorkflowSequence& sequence : m_workflow.sequences) if (sequence.transition.targetSequenceIndex >= m_workflow.sequences.size()) sequence.transition.enabled = false;
    markDirty(); loadSequenceEditor();
}

void MainWindow::moveSequence(int delta)
{
    if (!hasCurrentSequence()) return;
    const int target = m_currentSequenceIndex + delta;
    if (target < 0 || target >= m_workflow.sequences.size()) return;
    m_workflow.sequences.swapItemsAt(m_currentSequenceIndex, target);
    for (int index = 0; index < m_workflow.sequences.size(); ++index) {
        VisualTransition& transition = m_workflow.sequences[index].transition;
        if (!transition.enabled) continue;
        int destination = transition.targetSequenceIndex;
        if (destination == m_currentSequenceIndex) destination = target;
        else if (destination == target) destination = m_currentSequenceIndex;
        transition.targetSequenceIndex = destination;
        if (transition.targetSequenceIndex <= index) transition.enabled = false;
    }
    m_currentSequenceIndex = target;
    markDirty();
    loadSequenceEditor();
}

void MainWindow::onAddPoint()
{
    if (!hasCurrentSequence()) return;
    SequencePoint point{};
    const QPoint cursor = m_platform->getMousePosition();
    point.screenX = cursor.x(); point.screenY = cursor.y(); point.button = RecordedButton::Left; point.intervalMs = 100; point.clickCount = 1; point.clickIntervalMs = 50;
    point.windowId = m_platform->getWindowAtPoint(point.screenX, point.screenY);
    if (!point.windowId) { QMessageBox::warning(this, QStringLiteral("无法绑定窗口"), QStringLiteral("该坐标没有可绑定窗口，请检查坐标后重试。")); return; }
    const WindowInfo info = m_platform->getWindowInfo(point.windowId);
    point.windowTitle = info.title; point.processName = info.processName; point.windowClass = info.className;
    point.relX = point.screenX - info.x; point.relY = point.screenY - info.y; point.boundWindowWidth = info.width; point.boundWindowHeight = info.height; point.hasWindowBinding = true;
    currentSequence().points.append(point); synchronizeWorkflowTargetWindow(); markDirty(); refreshPoints();
}
void MainWindow::onRecordToggled(bool enabled)
{
    if (enabled && !hasCurrentSequence()) return;
    m_recording = enabled;
    if (enabled) {
        m_lastRecordTime = QDateTime();
        if (!m_platform->startMouseHook([this](const RecordedClick& click) { onRecorded(click); })) { m_recording = false; return; }
        m_recordButton->setText(QStringLiteral("停止录制"));
        statusBar()->showMessage(QStringLiteral("正在录制：点击目标窗口添加点位。"));
    } else { m_platform->stopMouseHook(); m_recordButton->setText(QStringLiteral("开始录制")); statusBar()->showMessage(QStringLiteral("录制已停止。"), 3000); }
}
void MainWindow::onRecorded(const RecordedClick& click)
{
    if (!m_recording || !hasCurrentSequence()) return;
    SequencePoint point{}; point.screenX = click.x; point.screenY = click.y; point.button = click.button; point.windowId = click.windowId; point.clickCount = 1; point.clickIntervalMs = 50; point.intervalMs = 100;
    const WindowInfo info = m_platform->getWindowInfo(click.windowId); point.windowTitle = info.title; point.processName = info.processName; point.windowClass = info.className; point.relX = click.x - info.x; point.relY = click.y - info.y; point.boundWindowWidth = info.width; point.boundWindowHeight = info.height; point.hasWindowBinding = info.id != 0;
    const QDateTime now = QDateTime::currentDateTime(); if (m_lastRecordTime.isValid() && !currentSequence().points.isEmpty()) currentSequence().points.last().intervalMs = qMax(1, int(m_lastRecordTime.msecsTo(now))); m_lastRecordTime = now;
    currentSequence().points.append(point); synchronizeWorkflowTargetWindow(); markDirty(); refreshPoints();
}
void MainWindow::deletePoint() { const int row = m_points->currentRow(); if (hasCurrentSequence() && row >= 0) { currentSequence().points.removeAt(row); synchronizeWorkflowTargetWindow(); markDirty(); refreshPoints(); } }
void MainWindow::deleteAllPoints()
{
    if (!hasCurrentSequence() || currentSequence().points.isEmpty()) return;
    if (QMessageBox::question(this, QStringLiteral("删除全部点位"), QStringLiteral("确定删除当前序列的全部点击点？"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    currentSequence().points.clear();
    synchronizeWorkflowTargetWindow();
    markDirty();
    refreshPoints();
}
void MainWindow::movePoint(int delta)
{
    if (!hasCurrentSequence()) return;
    const int row = m_points->currentRow();
    const int target = row + delta;
    if (row < 0 || target < 0 || target >= currentSequence().points.size()) return;
    currentSequence().points.swapItemsAt(row, target);
    markDirty();
    refreshPoints();
    if (target >= 0 && target < m_points->rowCount()) m_points->selectRow(target);
}
void MainWindow::editPoint()
{
    const int row = m_points->currentRow(); if (!hasCurrentSequence() || row < 0 || row >= currentSequence().points.size()) return; SequencePoint& point = currentSequence().points[row];
    int editGlobalX = point.screenX, editGlobalY = point.screenY; if (pointHasWindowBinding(point)) { const uintptr_t id = m_platform->findWindowByLocator(point.windowTitle, point.processName, point.windowClass); if (id) { const WindowInfo info = m_platform->isWindowMinimized(id) ? m_platform->getWindowPlacementInfo(id) : m_platform->getWindowInfo(id); const ResolvedPointGeometry resolved = resolveSequencePoint(point, &info); editGlobalX = resolved.globalX; editGlobalY = resolved.globalY; } }
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("编辑点击点")); QFormLayout layout(&dialog); QSpinBox x, y, count, clickInterval, nextInterval; QComboBox button; button.addItem(QStringLiteral("左键"), int(RecordedButton::Left)); button.addItem(QStringLiteral("右键"), int(RecordedButton::Right)); button.addItem(QStringLiteral("中键"), int(RecordedButton::Middle)); button.setCurrentIndex(button.findData(int(point.button))); x.setRange(-100000, 100000); y.setRange(-100000, 100000); count.setRange(1, 100000); clickInterval.setRange(1, 60000); nextInterval.setRange(1, 60000); x.setValue(editGlobalX); y.setValue(editGlobalY); count.setValue(qMax(1, point.clickCount)); clickInterval.setValue(qMax(1, point.clickIntervalMs)); nextInterval.setValue(qMax(1, point.intervalMs));
    QComboBox window; window.addItem(QStringLiteral("不绑定窗口"), QVariant::fromValue<qulonglong>(0)); const auto windows = m_platform->listWindows(); int selected = 0; for (const WindowInfo& info : windows) { QString label = QString::fromStdWString(info.title); if (label.isEmpty()) label = QString::fromStdWString(info.className); if (label.isEmpty()) label = QStringLiteral("窗口"); label += QStringLiteral(" [%1]").arg(QString::fromStdWString(info.processName)); window.addItem(label, QVariant::fromValue<qulonglong>(static_cast<qulonglong>(info.id))); if (point.windowId == info.id) selected = window.count() - 1; } window.setCurrentIndex(selected);
    layout.addRow(QStringLiteral("屏幕 X"), &x); layout.addRow(QStringLiteral("屏幕 Y"), &y); layout.addRow(QStringLiteral("鼠标按钮"), &button); layout.addRow(QStringLiteral("绑定窗口"), &window); layout.addRow(QStringLiteral("点击次数"), &count); layout.addRow(QStringLiteral("重复点击间隔 (ms)"), &clickInterval); layout.addRow(QStringLiteral("下一点间隔 (ms)"), &nextInterval); QDialogButtonBox box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); layout.addRow(&box); connect(&box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(&box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
        point.screenX = x.value(); point.screenY = y.value(); point.button = static_cast<RecordedButton>(button.currentData().toInt()); point.clickCount = count.value(); point.clickIntervalMs = clickInterval.value(); point.intervalMs = nextInterval.value(); const uintptr_t id = static_cast<uintptr_t>(window.currentData().toULongLong());
        if (!id) { point.windowId = 0; point.windowTitle.clear(); point.processName.clear(); point.windowClass.clear(); point.relX = point.relY = 0; point.boundWindowWidth = point.boundWindowHeight = 0; point.hasWindowBinding = false; }
        else { const WindowInfo info = m_platform->isWindowMinimized(id) ? m_platform->getWindowPlacementInfo(id) : m_platform->getWindowInfo(id); const bool sameBinding = pointHasWindowBinding(point) && point.windowTitle == info.title && point.processName == info.processName && point.windowClass == info.className; const int referenceWidth = sameBinding && point.boundWindowWidth > 0 ? point.boundWindowWidth : info.width; const int referenceHeight = sameBinding && point.boundWindowHeight > 0 ? point.boundWindowHeight : info.height; point.windowId = id; point.windowTitle = info.title; point.processName = info.processName; point.windowClass = info.className; point.boundWindowWidth = referenceWidth; point.boundWindowHeight = referenceHeight; point.relX = info.width > 0 ? qRound((point.screenX - info.x) * double(referenceWidth) / info.width) : point.screenX - info.x; point.relY = info.height > 0 ? qRound((point.screenY - info.y) * double(referenceHeight) / info.height) : point.screenY - info.y; point.hasWindowBinding = true; }
        synchronizeWorkflowTargetWindow(); markDirty(); refreshPoints();
    }
}

void MainWindow::unbindCurrentSequencePoints()
{
    if (!hasCurrentSequence() || QMessageBox::question(this, QStringLiteral("取消窗口绑定"), QStringLiteral("取消当前序列的全部点位绑定？点位将改为屏幕绝对坐标。"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    for (SequencePoint& point : currentSequence().points) { point.windowId = 0; point.windowTitle.clear(); point.processName.clear(); point.windowClass.clear(); point.relX = point.relY = 0; point.boundWindowWidth = point.boundWindowHeight = 0; point.hasWindowBinding = false; }
    markDirty(); refreshPoints(); synchronizeWorkflowTargetWindow();
}

void MainWindow::synchronizeWorkflowTargetWindow()
{
    for (const WorkflowSequence& source : m_workflow.sequences) {
        for (const SequencePoint& point : source.points) {
            if (!point.hasWindowBinding && point.windowId == 0 && point.windowTitle.empty() && point.processName.empty() && point.windowClass.empty()) continue;
            m_workflow.targetWindow.title = QString::fromStdWString(point.windowTitle);
            m_workflow.targetWindow.processName = QString::fromStdWString(point.processName);
            m_workflow.targetWindow.className = QString::fromStdWString(point.windowClass);
            const QString name = m_workflow.targetWindow.title.isEmpty() ? m_workflow.targetWindow.className : m_workflow.targetWindow.title;
            m_windowLabel->setText(QStringLiteral("自动绑定：%1").arg(name));
            return;
        }
    }
    if (!m_workflow.targetWindow.title.isEmpty() || !m_workflow.targetWindow.processName.isEmpty() || !m_workflow.targetWindow.className.isEmpty()) {
        const QString name = !m_workflow.targetWindow.title.isEmpty() ? m_workflow.targetWindow.title : m_workflow.targetWindow.className;
        m_windowLabel->setText(QStringLiteral("自动绑定：%1").arg(name));
        return;
    }
    if (m_workflow.sequences.isEmpty()) {
        m_windowLabel->setText(QStringLiteral("由序列点自动绑定（尚未添加点位）"));
        return;
    }
    m_windowLabel->setText(QStringLiteral("由序列点自动绑定（点位尚未关联窗口）"));
}
bool MainWindow::resolveWorkflowWindow(uintptr_t* id) const
{
    if (*id) return true;
    *id = m_platform->findWindowByLocator(m_workflow.targetWindow.title.toStdWString(), m_workflow.targetWindow.processName.toStdWString(), m_workflow.targetWindow.className.toStdWString());
    if (*id) return true;
    const auto resolvePoint = [this](const WorkflowSequence& sequence) -> uintptr_t {
        for (const SequencePoint& point : sequence.points) {
            if (!point.hasWindowBinding && point.windowId == 0 && point.windowTitle.empty() && point.processName.empty() && point.windowClass.empty()) continue;
            const uintptr_t found = m_platform->findWindowByLocator(point.windowTitle, point.processName, point.windowClass);
            if (found) return found;
        }
        return 0;
    };
    if (hasCurrentSequence() && (*id = resolvePoint(currentSequence()))) return true;
    for (const WorkflowSequence& sequence : m_workflow.sequences) if ((*id = resolvePoint(sequence))) return true;
    return false;
}
bool MainWindow::resolveSequenceWindow(const WorkflowSequence& sequence, uintptr_t* id) const
{
    *id = 0;
    for (const SequencePoint& point : sequence.points) {
        if (!point.hasWindowBinding && point.windowId == 0 && point.windowTitle.empty() && point.processName.empty() && point.windowClass.empty()) continue;
        *id = m_platform->findWindowByLocator(point.windowTitle, point.processName, point.windowClass);
        if (*id) return true;
    }
    return resolveWorkflowWindow(id);
}

void MainWindow::configureTransition()
{
    if (!hasCurrentSequence()) return;
    if (m_currentSequenceIndex >= m_workflow.sequences.size() - 1) { QMessageBox::information(this, QStringLiteral("最后序列"), QStringLiteral("最后一个序列不能设置后续跳转。")); return; }
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("配置跳转条件")); dialog.setMinimumWidth(360); QFormLayout layout(&dialog); layout.setContentsMargins(8, 8, 8, 8); layout.setVerticalSpacing(4);
    QComboBox type; type.addItem(QStringLiteral("图片出现"), int(VisualConditionType::Template)); type.addItem(QStringLiteral("颜色占比"), int(VisualConditionType::ColorRatio)); type.addItem(QStringLiteral("循环结束自动跳转"), int(VisualConditionType::LoopEnd)); type.setCurrentIndex(type.findData(int(currentSequence().transition.type)));
    QComboBox target; for (int i = m_currentSequenceIndex + 1; i < m_workflow.sequences.size(); ++i) target.addItem(m_workflow.sequences[i].name, i);
    QSpinBox similarity; similarity.setRange(50, 100); similarity.setValue(int(currentSequence().transition.templateCondition.similarity * 100));
    QPushButton chooseImage(QStringLiteral("导入模板图片")); QPushButton captureImage(QStringLiteral("从窗口截图模板")); QPushButton drawRegion(QStringLiteral("绘制多边形区域并取色")); QLabel imageState(currentSequence().transition.templateCondition.pngData.isEmpty() ? QStringLiteral("未选择图片") : QStringLiteral("已选择图片"));
    auto* imagePreview = new ClickableImageLabel(&dialog); imagePreview->setMinimumSize(160, 96); imagePreview->setMaximumSize(220, 132);
    QSpinBox red, green, blue, tolerance, ratio; red.setRange(0,255); green.setRange(0,255); blue.setRange(0,255); tolerance.setRange(0,255); ratio.setRange(0,100); const QColor color = currentSequence().transition.colorCondition.color; red.setValue(color.red()); green.setValue(color.green()); blue.setValue(color.blue()); tolerance.setValue(currentSequence().transition.colorCondition.tolerance); ratio.setValue(int(currentSequence().transition.colorCondition.ratio * 100));
    QComboBox compare; compare.addItem(QStringLiteral("占比低于/等于"), false); compare.addItem(QStringLiteral("占比高于/等于"), true);
    layout.addRow(QStringLiteral("条件类型"), &type); layout.addRow(QStringLiteral("跳转到"), &target); layout.addRow(QStringLiteral("模板相似度 (%)"), &similarity); layout.addRow(&chooseImage, &imageState); layout.addRow(imagePreview); layout.addRow(&captureImage); layout.addRow(&drawRegion); layout.addRow(QStringLiteral("颜色 R"), &red); layout.addRow(QStringLiteral("颜色 G"), &green); layout.addRow(QStringLiteral("颜色 B"), &blue); layout.addRow(QStringLiteral("颜色容差"), &tolerance); layout.addRow(QStringLiteral("颜色占比 (%)"), &ratio); layout.addRow(QStringLiteral("比较"), &compare);
    QByteArray imageData = currentSequence().transition.templateCondition.pngData;
    auto updateImagePreview = [&]() {
        QImage image; image.loadFromData(imageData);
        if (image.isNull()) { imagePreview->setImage({}); imageState.setText(QStringLiteral("未选择图片")); return; }
        imagePreview->setImage(image);
        imageState.setText(QStringLiteral("已选择图片（%1 × %2）").arg(image.width()).arg(image.height()));
    };
    imagePreview->setClickHandler([&dialog, imagePreview]() {
        if (imagePreview->image().isNull()) return;
        auto* preview = new ImagePreviewDialog(imagePreview->image(), &dialog);
        preview->setAttribute(Qt::WA_DeleteOnClose);
        preview->show();
        preview->raise();
        preview->activateWindow();
    });
    updateImagePreview();
    connect(&chooseImage, &QPushButton::clicked, &dialog, [&]() { const QString path = QFileDialog::getOpenFileName(&dialog, QStringLiteral("导入模板"), QString(), QStringLiteral("图片 (*.png *.jpg *.jpeg)")); if (path.isEmpty()) return; QImage image(path); if (image.isNull()) { imageState.setText(QStringLiteral("图片无效")); return; } QBuffer buffer(&imageData); buffer.open(QIODevice::WriteOnly); image.save(&buffer, "PNG"); updateImagePreview(); });
    QVector<QPointF> regionPoints = currentSequence().transition.region.points;
    auto capture = [&]() -> QImage { uintptr_t id = 0; if (!resolveSequenceWindow(currentSequence(), &id)) { QMessageBox::warning(&dialog, QStringLiteral("目标窗口不可用"), QStringLiteral("请先录制或添加至少一个已绑定窗口的点击点。")); return {}; } QImage image = m_platform->captureWindow(id); if (image.isNull()) QMessageBox::warning(&dialog, QStringLiteral("截图失败"), QStringLiteral("目标窗口必须处于前台、未最小化且不被遮挡。")); return image; };
    connect(&captureImage, &QPushButton::clicked, &dialog, [&]() { const QImage image = capture(); if (image.isNull()) return; RegionDialog picker(image, RegionCanvas::Mode::Rectangle, &dialog); if (picker.exec() != QDialog::Accepted || !picker.valid()) return; QImage crop = picker.crop(); QBuffer buffer(&imageData); buffer.open(QIODevice::WriteOnly); crop.save(&buffer, "PNG"); updateImagePreview(); });
    connect(&drawRegion, &QPushButton::clicked, &dialog, [&]() { const QImage image = capture(); if (image.isNull()) return; RegionDialog picker(image, RegionCanvas::Mode::Polygon, &dialog); if (picker.exec() != QDialog::Accepted || !picker.valid()) return; regionPoints = picker.polygon(); const QColor picked = picker.color(); if (picked.isValid()) { red.setValue(picked.red()); green.setValue(picked.green()); blue.setValue(picked.blue()); } });
    auto setRowVisible = [&layout](QWidget* field, bool visible) { if (QWidget* label = layout.labelForField(field)) label->setVisible(visible); field->setVisible(visible); };
    auto updateEditor = [&]() {
        const bool isTemplate = type.currentData().toInt() == int(VisualConditionType::Template);
        const bool isColor = type.currentData().toInt() == int(VisualConditionType::ColorRatio);
        setRowVisible(&similarity, isTemplate); chooseImage.setVisible(isTemplate); imageState.setVisible(isTemplate); imagePreview->setVisible(isTemplate); captureImage.setVisible(isTemplate);
        setRowVisible(&drawRegion, isColor); setRowVisible(&red, isColor); setRowVisible(&green, isColor); setRowVisible(&blue, isColor); setRowVisible(&tolerance, isColor); setRowVisible(&ratio, isColor); setRowVisible(&compare, isColor);
    };
    connect(&type, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, [=](int) { updateEditor(); });
    updateEditor();
    QDialogButtonBox box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); layout.addRow(&box);
    connect(&box, &QDialogButtonBox::accepted, &dialog, [&]() {
        const bool isTemplate = type.currentData().toInt() == int(VisualConditionType::Template);
        const bool isColor = type.currentData().toInt() == int(VisualConditionType::ColorRatio);
        if (isTemplate && imageData.isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("模板未设置"), QStringLiteral("请选择或截取用于检测的模板图片。")); return; }
        if (isColor && regionPoints.size() < 3) { QMessageBox::warning(&dialog, QStringLiteral("区域未设置"), QStringLiteral("颜色占比条件必须绘制至少三个顶点的多边形区域。")); return; }
        dialog.accept();
    });
    connect(&box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    VisualTransition& transition = currentSequence().transition; transition.enabled = true; transition.targetSequenceIndex = target.currentData().toInt(); transition.type = VisualConditionType(type.currentData().toInt()); transition.requiredHits = 1; transition.templateCondition.expectPresent = true; transition.templateCondition.similarity = similarity.value() / 100.0; transition.templateCondition.pngData = imageData; transition.colorCondition.color = QColor(red.value(), green.value(), blue.value()); transition.colorCondition.tolerance = tolerance.value(); transition.colorCondition.ratio = ratio.value() / 100.0; transition.colorCondition.atLeast = compare.currentData().toBool();
    if (transition.type == VisualConditionType::ColorRatio) transition.region.points = regionPoints;
    markDirty(); loadSequenceEditor();
}

void MainWindow::startWorkflow()
{
    synchronizeWorkflowTargetWindow();
    const int startIndex = qBound(0, m_startSequenceIndex, qMax(0, m_workflow.sequences.size() - 1));
    if (m_workflow.sequences.isEmpty() || m_workflow.sequences[startIndex].points.isEmpty()) { QMessageBox::warning(this, QStringLiteral("无法开始"), QStringLiteral("起始序列没有点击点。")); return; }
    uintptr_t id = 0;
    for (const SequencePoint& point : m_workflow.sequences[startIndex].points) {
        if (!point.hasWindowBinding && point.windowId == 0 && point.windowTitle.empty() && point.processName.empty() && point.windowClass.empty()) continue;
        id = m_platform->findWindowByLocator(point.windowTitle, point.processName, point.windowClass);
        if (id) break;
    }
    const bool resolvedWindow = resolveWorkflowWindow(&id);
    for (int index = 0; index < m_workflow.sequences.size(); ++index) {
        const VisualTransition& transition = m_workflow.sequences[index].transition;
        if (!transition.enabled) continue;
        if (transition.targetSequenceIndex <= index || transition.targetSequenceIndex >= m_workflow.sequences.size() || (transition.type == VisualConditionType::ColorRatio && transition.region.points.size() < 3)) { QMessageBox::warning(this, QStringLiteral("条件配置无效"), QStringLiteral("序列“%1”的跳转目标或颜色检测区域无效。").arg(m_workflow.sequences[index].name)); return; }
        if (transition.type == VisualConditionType::Template && transition.templateCondition.pngData.isEmpty()) { QMessageBox::warning(this, QStringLiteral("条件配置无效"), QStringLiteral("序列“%1”的图片条件尚未选择模板。").arg(m_workflow.sequences[index].name)); return; }
        if (transition.type == VisualConditionType::LoopEnd && m_workflow.sequences[index].loopCount == -1) { QMessageBox::warning(this, QStringLiteral("条件配置无效"), QStringLiteral("序列“%1”设置为无限循环，无法触发循环结束跳转。").arg(m_workflow.sequences[index].name)); return; }
    }
    bool needsVisualWindow = false;
    for (const WorkflowSequence& sequence : m_workflow.sequences) {
        if (sequence.transition.enabled && sequence.transition.type != VisualConditionType::LoopEnd) { needsVisualWindow = true; break; }
    }
    const bool backgroundMode = m_workflow.clickMethod == ClickMethod::NoInterference;
    if (backgroundMode && needsVisualWindow) { QMessageBox::warning(this, QStringLiteral("无法后台运行"), QStringLiteral("无干扰后台模式不支持视觉条件，请改用模拟鼠标或移除视觉条件。")); return; }
    if (!resolvedWindow && needsVisualWindow) { QMessageBox::warning(this, QStringLiteral("视觉条件不可用"), QStringLiteral("图片或颜色条件需要至少一个自动绑定的可见窗口。")); return; }
    if (!backgroundMode && id && !m_platform->setForegroundWindow(id)) { QMessageBox::warning(this, QStringLiteral("无法激活目标窗口"), QStringLiteral("请手动将序列点绑定的窗口切到前台后按 F6 开始。")); return; }
    m_engine->setWorkflow(m_workflow, id, startIndex); m_engine->setClickMethod(m_workflow.clickMethod); m_engine->start();
    if (m_engine->isRunning()) updateRunStatus(QStringLiteral("正在启动…"), QStringLiteral("#047857"));
    updateRunControls();
}
void MainWindow::stopWorkflow() { if (!m_engine->isRunning()) return; m_engine->stop(); updateRunStatus(QStringLiteral("已停止")); updateRunControls(); }
void MainWindow::pauseWorkflow() { if (!m_engine->isRunning()) return; if (m_engine->isPaused()) { m_engine->resume(); updateRunStatus(QStringLiteral("已恢复"), QStringLiteral("#047857")); } else { m_engine->pause(); updateRunStatus(QStringLiteral("已暂停"), QStringLiteral("#b45309")); } updateRunControls(); }
