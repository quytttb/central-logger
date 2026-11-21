#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtQmlIntegration/qqmlintegration.h>

namespace CentralLogger::Core {

struct LoggerLiveListEntry
{
    qint64      loggerId      = 0;
    QString     loggerName;
    QString     stationCode;
    QString     host;
    bool        online        = false;
    int         edgeSensorId  = 0;
    QString     sensorName;
    QString     sensorType;
    QString     value;
    QString     unit;
    QString     displayStatus;
    bool        status03Auto  = false;
    QString     alarmType;
    QStringList attachDiTypeCodes;
    QStringList attachDiTypeLabels;
    bool        hasLimit      = false;
    QString     limitText;
    bool        valid         = false;

    bool sameIdentity(const LoggerLiveListEntry &other) const
    {
        return loggerId == other.loggerId
            && sensorType == other.sensorType
            && edgeSensorId == other.edgeSensorId;
    }

    bool operator==(const LoggerLiveListEntry &other) const = default;
};

/// Flat multi-logger sensor table for `Loggers > Data` List mode.
/// Columns: Logger · Sensor · Value · Unit · Status · Limit.
/// Updates in-place via `dataChanged` whenever row identities are unchanged so
/// live polling never resets scroll position.
class LoggerLiveListModel : public QAbstractTableModel
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    enum Column {
        LoggerColumn = 0,
        SensorColumn,
        ValueColumn,
        UnitColumn,
        StatusColumn,
        LimitColumn,
        ColumnCount,
    };

    enum Roles {
        LoggerIdRole = Qt::UserRole + 1,
        LoggerNameRole,
        StationCodeRole,
        OnlineRole,
        EdgeSensorIdRole,
        NameRole,
        SensorTypeRole,
        ValueRole,
        UnitRole,
        DisplayStatusRole,
        Status03AutoRole,
        AlarmTypeRole,
        AttachDiCodesRole,
        AttachDiLabelsRole,
        HasLimitRole,
        LimitTextRole,
        ValidRole,
    };

    explicit LoggerLiveListModel(QObject *parent = nullptr);

    int      rowCount(const QModelIndex &parent = {}) const override;
    int      columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const { return m_filterText; }
    Q_INVOKABLE void setFilterText(const QString &text);

    void setEntries(QVector<LoggerLiveListEntry> entries);

signals:
    void countChanged();
    void filterTextChanged();

private:
    void rebuildVisible();

    QVector<LoggerLiveListEntry> m_allEntries;
    QVector<LoggerLiveListEntry> m_visibleEntries;
    QString                      m_filterText;
};

} // namespace CentralLogger::Core
