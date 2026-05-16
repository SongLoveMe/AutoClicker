#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QList>
#include <QPoint>
#include "PlatformAdapter.h"

struct AppConfig {
    // Click settings
    ClickMode mode = ClickMode::FixedPosition;
    MouseButton button = MouseButton::Left;
    ClickAction action = ClickAction::Single;
    ClickMethod clickMethod = ClickMethod::NoInterference;  // v2.2: restored for game compatibility
    int intervalBase = 100;       // Base interval in ms
    int jitterRange = 10;         // Jitter range in ms (+/-)
    bool useRandomize = false;    // Combined: interval jitter + position offset
    int clickCount = -1;
    // Removed: antiDetect - merged into useRandomize

    // Position settings
    int targetX = 0;
    int targetY = 0;
    QList<QPoint> sequencePoints;

    // Hotkey settings
    int startStopHotkey = Qt::Key_F6;
    int pauseHotkey = Qt::Key_F7;

    // Window settings
    QString bindWindowTitle;
    bool bindToWindow = false;
    // Removed: stayOnTop - always true now

    // General settings
    QString language = "en";
    bool minimizeOnStart = false;
    bool showInTaskbar = true;
};

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject* parent = nullptr);
    ~ConfigManager();

    bool loadConfig(const QString& filePath);
    bool saveConfig(const QString& filePath);
    bool loadDefaultConfig();
    bool saveDefaultConfig();

    AppConfig getConfig() const;
    void setConfig(const AppConfig& config);

    QString getDefaultConfigPath() const;
    QString getUserConfigPath() const;

private:
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);

    AppConfig m_config;
    QString m_configPath;
};

#endif // CONFIGMANAGER_H