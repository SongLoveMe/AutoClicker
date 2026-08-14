#ifndef WORKFLOW_H
#define WORKFLOW_H

#include <QByteArray>
#include <QColor>
#include <QPointF>
#include <QString>
#include <QUuid>
#include <QVector>
#include "PlatformAdapter.h"

enum class VisualConditionType { Template, ColorRatio, LoopEnd };

struct WindowLocator {
    QString title;
    QString processName;
    QString className;
};

struct PolygonRegion {
    QVector<QPointF> points; // Coordinates relative to target window.
};

struct TemplateCondition {
    QByteArray pngData;
    bool expectPresent = true;
    double similarity = 0.90;
};

struct ColorRatioCondition {
    QColor color = QColor(0, 200, 0);
    int tolerance = 20;
    bool atLeast = false;
    double ratio = 0.50;
};

struct VisualTransition {
    bool enabled = false;
    int targetSequenceIndex = -1;
    VisualConditionType type = VisualConditionType::Template;
    PolygonRegion region;
    TemplateCondition templateCondition;
    ColorRatioCondition colorCondition;
    int requiredHits = 1;
};

struct WorkflowSequence {
    QString name;
    QList<SequencePoint> points;
    int loopCount = 1; // -1 means infinite
    VisualTransition transition;
};

struct Workflow {
    int formatVersion = 3;
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString name = QStringLiteral("未命名工作流");
    WindowLocator targetWindow;
    ClickMethod clickMethod = ClickMethod::SimulateMouse;
    int detectionIntervalMs = 500;
    QList<WorkflowSequence> sequences;
};

#endif
