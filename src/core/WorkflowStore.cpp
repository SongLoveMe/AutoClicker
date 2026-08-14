#include "WorkflowStore.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {
QJsonObject pointToJson(const SequencePoint& point) {
    QJsonObject value;
    value["screenX"] = point.screenX; value["screenY"] = point.screenY;
    value["button"] = static_cast<int>(point.button); value["windowId"] = QString::number(point.windowId);
    value["relX"] = point.relX; value["relY"] = point.relY;
    value["windowTitle"] = QString::fromStdWString(point.windowTitle);
    value["processName"] = QString::fromStdWString(point.processName);
    value["windowClass"] = QString::fromStdWString(point.windowClass);
    value["windowBound"] = point.hasWindowBinding;
    value["boundWindowWidth"] = point.boundWindowWidth; value["boundWindowHeight"] = point.boundWindowHeight;
    value["label"] = point.userLabel; value["intervalMs"] = point.intervalMs;
    value["clickCount"] = point.clickCount; value["clickIntervalMs"] = point.clickIntervalMs;
    return value;
}
SequencePoint pointFromJson(const QJsonObject& value) {
    SequencePoint point{};
    point.screenX = value["screenX"].toInt(); point.screenY = value["screenY"].toInt();
    point.button = static_cast<RecordedButton>(value["button"].toInt());
    point.windowId = value["windowId"].toString().toULongLong();
    point.relX = value["relX"].toInt(); point.relY = value["relY"].toInt();
    point.windowTitle = value["windowTitle"].toString().toStdWString();
    point.processName = value["processName"].toString().toStdWString();
    point.windowClass = value["windowClass"].toString().toStdWString();
    point.hasWindowBinding = value["windowBound"].toBool(!point.windowTitle.empty() || !point.processName.empty() || !point.windowClass.empty() || point.windowId != 0);
    point.boundWindowWidth = value["boundWindowWidth"].toInt(); point.boundWindowHeight = value["boundWindowHeight"].toInt();
    point.userLabel = value["label"].toString(); point.intervalMs = value.contains("intervalMs") ? value["intervalMs"].toInt(100) : 100;
    point.clickCount = qMax(1, value["clickCount"].toInt(1)); point.clickIntervalMs = qMax(1, value["clickIntervalMs"].toInt(50));
    return point;
}
QJsonArray polygonToJson(const PolygonRegion& region) {
    QJsonArray result;
    for (const QPointF& p : region.points) result.append(QJsonObject{{"x", p.x()}, {"y", p.y()}});
    return result;
}
PolygonRegion polygonFromJson(const QJsonArray& values) {
    PolygonRegion result;
    for (const QJsonValue& value : values) { QJsonObject p = value.toObject(); result.points.append(QPointF(p["x"].toDouble(), p["y"].toDouble())); }
    return result;
}
}

bool WorkflowStore::save(const Workflow& workflow, const QString& path, QString* error) {
    QJsonObject root{{"formatVersion", workflow.formatVersion}, {"id", workflow.id}, {"name", workflow.name},
                     {"clickMethod", static_cast<int>(workflow.clickMethod)}, {"detectionIntervalMs", workflow.detectionIntervalMs},
                     {"targetWindow", QJsonObject{{"title", workflow.targetWindow.title}, {"processName", workflow.targetWindow.processName}, {"className", workflow.targetWindow.className}}}};
    QJsonArray sequences;
    for (const WorkflowSequence& sequence : workflow.sequences) {
        QJsonObject transition{{"enabled", sequence.transition.enabled}, {"targetSequenceIndex", sequence.transition.targetSequenceIndex},
            {"type", static_cast<int>(sequence.transition.type)}, {"requiredHits", sequence.transition.requiredHits},
            {"region", polygonToJson(sequence.transition.region)},
            {"template", QJsonObject{{"png", QString::fromLatin1(sequence.transition.templateCondition.pngData.toBase64())},
                {"expectPresent", sequence.transition.templateCondition.expectPresent}, {"similarity", sequence.transition.templateCondition.similarity}}},
            {"color", QJsonObject{{"rgba", QString::number(sequence.transition.colorCondition.color.rgba())}, {"tolerance", sequence.transition.colorCondition.tolerance},
                {"atLeast", sequence.transition.colorCondition.atLeast}, {"ratio", sequence.transition.colorCondition.ratio}}}};
        QJsonArray points; for (const SequencePoint& point : sequence.points) points.append(pointToJson(point));
        sequences.append(QJsonObject{{"name", sequence.name}, {"loopCount", sequence.loopCount}, {"points", points}, {"transition", transition}});
    }
    root["sequences"] = sequences;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool WorkflowStore::load(const QString& path, Workflow* workflow, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { if (error) *error = file.errorString(); return false; }
    QJsonParseError parseError; const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) { if (error) *error = parseError.errorString(); return false; }
    const QJsonObject root = document.object();
    Workflow loaded; loaded.formatVersion = qMax(1, root["formatVersion"].toInt(1)); loaded.id = root["id"].toString();
    loaded.name = root["name"].toString(); loaded.detectionIntervalMs = root["detectionIntervalMs"].toInt(500);
    const int clickMethod = root["clickMethod"].toInt(static_cast<int>(ClickMethod::SimulateMouse));
    loaded.clickMethod = clickMethod == static_cast<int>(ClickMethod::NoInterference) ? ClickMethod::NoInterference : ClickMethod::SimulateMouse;
    const QJsonObject target = root["targetWindow"].toObject(); loaded.targetWindow.title = target["title"].toString(); loaded.targetWindow.processName = target["processName"].toString(); loaded.targetWindow.className = target["className"].toString();
    for (const QJsonValue& sequenceValue : root["sequences"].toArray()) {
        const QJsonObject object = sequenceValue.toObject(); WorkflowSequence sequence;
        sequence.name = object["name"].toString("序列"); sequence.loopCount = object["loopCount"].toInt(1);
        for (const QJsonValue& point : object["points"].toArray()) sequence.points.append(pointFromJson(point.toObject()));
        const QJsonObject transition = object["transition"].toObject(); sequence.transition.enabled = transition["enabled"].toBool();
        sequence.transition.targetSequenceIndex = transition["targetSequenceIndex"].toInt(-1); const int transitionType = transition["type"].toInt(0); sequence.transition.type = transitionType >= 0 && transitionType <= static_cast<int>(VisualConditionType::LoopEnd) ? static_cast<VisualConditionType>(transitionType) : VisualConditionType::Template;
        sequence.transition.requiredHits = qMax(1, transition["requiredHits"].toInt(1)); sequence.transition.region = polygonFromJson(transition["region"].toArray());
        const QJsonObject templ = transition["template"].toObject(); sequence.transition.templateCondition.pngData = QByteArray::fromBase64(templ["png"].toString().toLatin1());
        sequence.transition.templateCondition.expectPresent = templ["expectPresent"].toBool(true); sequence.transition.templateCondition.similarity = templ["similarity"].toDouble(0.90);
        const QJsonObject color = transition["color"].toObject(); sequence.transition.colorCondition.color = QColor::fromRgba(color["rgba"].toString().toUInt());
        sequence.transition.colorCondition.tolerance = color["tolerance"].toInt(20); sequence.transition.colorCondition.atLeast = color["atLeast"].toBool(false); sequence.transition.colorCondition.ratio = color["ratio"].toDouble(0.50);
        loaded.sequences.append(sequence);
    }
    if (loaded.id.isEmpty()) loaded.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (loaded.name.isEmpty() || loaded.sequences.isEmpty()) { if (error) *error = QStringLiteral("工作流文件不完整"); return false; }
    *workflow = loaded; return true;
}

bool WorkflowStore::loadLegacyConfig(const QString& path, Workflow* workflow, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { if (error) *error = file.errorString(); return false; }
    QJsonParseError parseError; const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) { if (error) *error = parseError.errorString(); return false; }
    const QJsonObject root = document.object(); const QJsonObject position = root["position"].toObject();
    Workflow migrated; migrated.name = QFileInfo(path).baseName() + QStringLiteral("（v2.3 导入）");
    WorkflowSequence sequence; sequence.name = QStringLiteral("序列 1"); sequence.loopCount = -1;
    const int interval = root["click"].toObject()["intervalBase"].toInt(100);
    for (const QJsonValue& value : position["sequence"].toArray()) {
        const QJsonObject pointValue = value.toObject(); SequencePoint point{}; point.screenX = pointValue["x"].toInt(); point.screenY = pointValue["y"].toInt(); point.button = RecordedButton::Left; point.intervalMs = interval; sequence.points.append(point);
    }
    if (sequence.points.isEmpty()) { SequencePoint point{}; point.screenX = position["targetX"].toInt(); point.screenY = position["targetY"].toInt(); point.button = RecordedButton::Left; point.intervalMs = interval; sequence.points.append(point); }
    migrated.sequences.append(sequence); *workflow = migrated; return true;
}

QString WorkflowStore::safeFileName(const QString& name) {
    QString cleaned = name; cleaned.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
    return cleaned.trimmed().isEmpty() ? QStringLiteral("workflow") : cleaned.trimmed();
}
