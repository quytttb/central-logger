#pragma once

#include "data/models/HistoryRow.h"

#include <QAbstractTableModel>
#include <QVector>
#include <QtQmlIntegration/qqmlintegration.h>

namespace CentralLogger::Core {

/// Read-only table model for the History view. Rows are a vector of
/// HistoryRow from SensorReadingRepository::searchHistory(). Thread-safe
/// writes via setRows() (UI thread only).
class HistoryTableModel : public QAbstractTableModel
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(int rowsSize READ rowsSize NOTIFY rowsSizeChanged)

public:
    enum Column {
        TimeColumn = 0,
        LoggerColumn,
        SensorColumn,
        UnitColumn,
        ValueColumn,
        StatusColumn,
        ColumnCount,
    };

    enum Role {
        TimeRole       = Qt::UserRole + 1,
        LoggerRole,
        SensorRole,
        UnitRole,
        ValueRole,
        StatusRole,     ///< "OK" | "ALARM" | "STALE" | "INVALID"
        ValidRole,
        AlarmRole,
        StaleRole,
        SensorIdRole,
    };
    Q_ENUM(Role)

    explicit HistoryTableModel(QObject *parent = nullptr);

    int      rowCount(const QModelIndex &parent = {}) const override;
    int      columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int rowsSize() const { return m_rows.size(); }

    /// Replaces all rows and emits a full reset.
    void setRows(const QVector<Data::HistoryRow> &rows);

    /// Clears all rows.
    void clear();

    const QVector<Data::HistoryRow> &rows() const { return m_rows; }

    static QString statusText(const Data::HistoryRow &r);

signals:
    void rowsSizeChanged();

private:

    QVector<Data::HistoryRow> m_rows;
};

} // namespace CentralLogger::Core
