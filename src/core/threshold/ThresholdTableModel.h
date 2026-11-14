#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <QVector>
#include <QtQmlIntegration/qqmlintegration.h>

namespace CentralLogger::Core {

/// One editable row in the Thresholds screen: an ANALOG sensor of the
/// selected logger plus its current min/max alarm thresholds as text.
struct ThresholdRow
{
    qint64  sensorId = 0;
    int     edgeSensorId = 0;
    QString name;
    QString unit;
    QString minText;
    QString maxText;
    /// Values loaded from the DB — used for dirty tracking and discard.
    QString originalMinText;
    QString originalMaxText;
};

/// Table model behind the Thresholds screen. Columns:
///   0 = sensor name (+ unit), 1 = min threshold, 2 = max threshold.
///
/// The model owns the full row list plus a visible-index subset so the
/// search box can filter without losing unsaved edits.
class ThresholdTableModel : public QAbstractTableModel
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(int  rowsSize   READ rowsSize   NOTIFY rowsSizeChanged)
    Q_PROPERTY(bool dirty      READ dirty      NOTIFY dirtyChanged)
    Q_PROPERTY(int  dirtyCount READ dirtyCount NOTIFY dirtyChanged)

public:
    enum Role {
        DisplayRole = Qt::DisplayRole,
        SensorIdRole = Qt::UserRole + 1,
        NameRole,
        UnitRole,
        MinTextRole,
        MaxTextRole,
        IsNameColumnRole,
        RowDirtyRole,
    };
    Q_ENUM(Role)

    explicit ThresholdTableModel(QObject *parent = nullptr);

    int      rowCount(const QModelIndex &parent = {}) const override;
    int      columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Replaces the full row list (resets the model).
    void setRows(const QVector<ThresholdRow> &rows);

    /// Case-insensitive filter over sensor name + unit.
    void setFilterText(const QString &text);

    /// Edits one cell from QML (column 1 = min, column 2 = max).
    Q_INVOKABLE void setCellText(int row, int column, const QString &text);

    /// Restores every cell to the value loaded from the DB.
    Q_INVOKABLE void resetEdits();

    /// Marks the current text as the new saved baseline (clears dirty).
    void markSaved();

    QVector<ThresholdRow> rows() const { return m_rows; }

    int  rowsSize() const { return m_visible.size(); }
    bool dirty() const;
    int  dirtyCount() const;

signals:
    void rowsSizeChanged();
    void dirtyChanged();

private:
    int  sourceRow(int visibleRow) const;
    void rebuildVisible();

    QVector<ThresholdRow> m_rows;
    QVector<int>          m_visible;
    QString               m_filterText;
};

} // namespace CentralLogger::Core
