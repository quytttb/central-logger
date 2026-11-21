#pragma once

#include <QAbstractListModel>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>

namespace CentralLogger::Core {

/// Keeps existing Canvas delegates alive when points or axis values change.
class GraphChartsModel : public QAbstractListModel {
    Q_OBJECT
    QML_ANONYMOUS

public:
    enum Role { ChartRole = Qt::UserRole + 1 };
    explicit GraphChartsModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Rows must have unique edgeSensorIds and be sorted by that key.
    void setCharts(qint64 loggerId, const QVariantList &charts);
    const QVariantList &charts() const { return m_charts; }

private:
    qint64 m_loggerId = 0;
    QVariantList m_charts;
};

} // namespace CentralLogger::Core
