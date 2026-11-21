#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QtQmlIntegration/qqmlintegration.h>

namespace CentralLogger::Core {

/// Per-logger section model for `Loggers > Data` (Cards mode).
/// Updates rows in-place via `dataChanged` whenever the visible logger ID
/// sequence is unchanged so live Modbus polling never resets scroll position.
class LoggerLiveCardsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    enum Roles {
        LoggerIdRole = Qt::UserRole + 1,
        NameRole,
        StationCodeRole,
        HostRole,
        EnabledRole,
        OnlineRole,
        PollingRole,
        AnyAlarmRole,
        SensorCountRole,
        LastUpdateRole,
        SensorsRole,
    };

    explicit LoggerLiveCardsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const { return m_filterText; }
    void setFilterText(const QString &text);
    void setSections(const QVariantList &sections);

signals:
    void countChanged();
    void filterTextChanged();

private:
    void rebuildVisible();

    QVector<QVariantMap> m_allSections;
    QVector<QVariantMap> m_sections;
    QString              m_filterText;
};

} // namespace CentralLogger::Core
