#include "settingscontroller.h"
#include "tools/configfilehandler.h"

#include <QQmlApplicationEngine>
#include <QRegularExpression>
#include <QQmlEngine>
#include <QDir>
#include <QQuickView>

extern QQmlApplicationEngine *gEngine;

SettingsController::SettingsController(ConfigFileHandler &configFileHandler)
    : m_ConfigFileModel(configFileHandler)
{
    m_HistoryPath = m_ConfigFileModel.getDeletionFilePath();
    m_RecursionDepth = m_ConfigFileModel.getRecursionDepth();
}

void SettingsController::setHistoryPath(const QString &newActivePath)
{
    qDebug() << "New active path has been set: " << newActivePath;
    QString validActivePath = newActivePath;
    validActivePath.remove(QRegularExpression("file:///"));
    validActivePath = QDir::cleanPath(validActivePath);
    qDebug() << "Edited active path has been set: " << validActivePath;

    QDir activePath(validActivePath);
    if (activePath.exists())
    {
        m_HistoryPath = validActivePath;
        emit historyPathChanged();
    } else {
        //show warning message
        qDebug() << "invalid path has been entered";
        emit historyPathInvalid();
    }
}

void SettingsController::setRecursionDepth(QString &newDepth)
{
    qDebug() << "New depth has been set: " << newDepth;
    m_RecursionDepth = QVariant(newDepth).toUInt();
    emit recursionDepthChanged();
}

QString SettingsController::HistoryPathToView() const
{
    return QDir::toNativeSeparators(m_HistoryPath);
}

QString SettingsController::getHistoryPath()
{
    return m_HistoryPath;
}

QString SettingsController::RecursionDepthToView() const
{
    return QVariant(m_RecursionDepth).toString();
}

void SettingsController::saveSettings()
{
    m_ConfigFileModel.setDeletionFilePath(m_HistoryPath);
    m_ConfigFileModel.setRecursionDepth(m_RecursionDepth);
    m_ConfigFileModel.writeSettings();
}

uint SettingsController::getRecursionDepth() const
{
    return m_RecursionDepth;
}
