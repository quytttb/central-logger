#pragma once

#include <QSortFilterProxyModel>
#include <QtQmlIntegration/qqmlintegration.h>

namespace CentralLogger::Core {

/// Search proxy over LoggerListModel (DashboardController.loggers).
/// Filters on name and host columns.  FE-004.
///
/// Exposed to QML as `LoggerSearchProxyModel`; the QML side sets
/// `filterText` and binds `sourceModel` to `DashboardController.loggers`.
class LoggerSearchProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    explicit LoggerSearchProxyModel(QObject *parent = nullptr);

    QString filterText() const { return m_filterText; }
    void setFilterText(const QString &text);

signals:
    void filterTextChanged();

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex &sourceParent) const override;

private:
    QString m_filterText;
};

} // namespace CentralLogger::Core
