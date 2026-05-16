#include "ConfigManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
    , m_configPath(getDefaultConfigPath())
{
}

ConfigManager::~ConfigManager()
{
}

QString ConfigManager::getDefaultConfigPath() const
{
    return "configs/default.json";
}

QString ConfigManager::getUserConfigPath() const
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dataPath + "/user_config.json";
}

bool ConfigManager::loadConfig(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open config file:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << error.errorString();
        return false;
    }

    fromJson(doc.object());
    m_configPath = filePath;

    qDebug() << "Config loaded from:" << filePath;
    return true;
}

bool ConfigManager::saveConfig(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot create config file:" << filePath;
        return false;
    }

    QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    m_configPath = filePath;
    qDebug() << "Config saved to:" << filePath;
    return true;
}

bool ConfigManager::loadDefaultConfig()
{
    QString defaultPath = getDefaultConfigPath();
    if (QFile::exists(defaultPath)) {
        return loadConfig(defaultPath);
    }
    return false;
}

bool ConfigManager::saveDefaultConfig()
{
    return saveConfig(getDefaultConfigPath());
}

AppConfig ConfigManager::getConfig() const
{
    return m_config;
}

void ConfigManager::setConfig(const AppConfig& config)
{
    m_config = config;
}

QJsonObject ConfigManager::toJson() const
{
    QJsonObject json;

    // Click settings
    QJsonObject clickSettings;
    clickSettings["mode"] = static_cast<int>(m_config.mode);
    clickSettings["button"] = static_cast<int>(m_config.button);
    clickSettings["action"] = static_cast<int>(m_config.action);
    clickSettings["intervalMin"] = m_config.intervalMin;
    clickSettings["intervalMax"] = m_config.intervalMax;
    clickSettings["clickCount"] = m_config.clickCount;
    clickSettings["antiDetect"] = m_config.antiDetect;
    json["click"] = clickSettings;

    // Position settings
    QJsonObject positionSettings;
    positionSettings["targetX"] = m_config.targetX;
    positionSettings["targetY"] = m_config.targetY;

    QJsonArray sequenceArray;
    for (const QPoint& pt : m_config.sequencePoints) {
        QJsonObject pointObj;
        pointObj["x"] = pt.x();
        pointObj["y"] = pt.y();
        sequenceArray.append(pointObj);
    }
    positionSettings["sequence"] = sequenceArray;
    json["position"] = positionSettings;

    // Hotkey settings
    QJsonObject hotkeySettings;
    hotkeySettings["startStop"] = m_config.startStopHotkey;
    hotkeySettings["pause"] = m_config.pauseHotkey;
    json["hotkey"] = hotkeySettings;

    // Window settings
    QJsonObject windowSettings;
    windowSettings["bindWindowTitle"] = m_config.bindWindowTitle;
    windowSettings["bindToWindow"] = m_config.bindToWindow;
    json["window"] = windowSettings;

    // General settings
    QJsonObject generalSettings;
    generalSettings["language"] = m_config.language;
    generalSettings["minimizeOnStart"] = m_config.minimizeOnStart;
    generalSettings["showInTaskbar"] = m_config.showInTaskbar;
    json["general"] = generalSettings;

    return json;
}

void ConfigManager::fromJson(const QJsonObject& json)
{
    // Click settings
    if (json.contains("click")) {
        QJsonObject click = json["click"].toObject();
        m_config.mode = static_cast<ClickMode>(click["mode"].toInt(0));
        m_config.button = static_cast<MouseButton>(click["button"].toInt(0));
        m_config.action = static_cast<ClickAction>(click["action"].toInt(0));
        m_config.intervalMin = click["intervalMin"].toInt(100);
        m_config.intervalMax = click["intervalMax"].toInt(100);
        m_config.clickCount = click["clickCount"].toInt(-1);
        m_config.antiDetect = click["antiDetect"].toBool(false);
    }

    // Position settings
    if (json.contains("position")) {
        QJsonObject position = json["position"].toObject();
        m_config.targetX = position["targetX"].toInt(0);
        m_config.targetY = position["targetY"].toInt(0);

        QJsonArray sequence = position["sequence"].toArray();
        m_config.sequencePoints.clear();
        for (const QJsonValue& val : sequence) {
            QJsonObject pt = val.toObject();
            m_config.sequencePoints.append(QPoint(pt["x"].toInt(), pt["y"].toInt()));
        }
    }

    // Hotkey settings
    if (json.contains("hotkey")) {
        QJsonObject hotkey = json["hotkey"].toObject();
        m_config.startStopHotkey = hotkey["startStop"].toInt(Qt::Key_F6);
        m_config.pauseHotkey = hotkey["pause"].toInt(Qt::Key_F7);
    }

    // Window settings
    if (json.contains("window")) {
        QJsonObject window = json["window"].toObject();
        m_config.bindWindowTitle = window["bindWindowTitle"].toString();
        m_config.bindToWindow = window["bindToWindow"].toBool(false);
    }

    // General settings
    if (json.contains("general")) {
        QJsonObject general = json["general"].toObject();
        m_config.language = general["language"].toString("en");
        m_config.minimizeOnStart = general["minimizeOnStart"].toBool(false);
        m_config.showInTaskbar = general["showInTaskbar"].toBool(true);
    }
}