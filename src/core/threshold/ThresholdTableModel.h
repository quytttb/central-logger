#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <QVector>
#include <QtQmlIntegration/qqmlintegration.h>

namespace CentralLogger::Core {

struct ThresholdRow
{
    QString name;
    QString unit;
    QString minText;
    QString maxText;
};

/// Read-only analog threshold table: sensor name (+ unit), min, max.
class ThresholdTableModel : public QAbstractTableModel
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(int rowsSize READ rowsSize NOTIFY rowsSizeChanged)

public:
    enum Role {
        UnitRole = Qt::UserRole + 1,
    };
    Q_ENUM(Role)

    explicit ThresholdTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(const QVector<ThresholdRow> &rows);
    void setFilterText(const QString &text);
    int rowsSize() const { return m_visible.size(); }

signals:
    void rowsSizeChanged();

private:
    void rebuildVisible();

    QVector<ThresholdRow> m_rows;
    QVector<int>          m_visible;
    QString               m_filterText;
};

} // namespace CentralLogger::Core
