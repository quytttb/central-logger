#include "core/sensors/LoggerLiveCardsModel.h"

#include "core/sensors/SensorLiveRow.h"
#include "utils/UiConstants.h"

namespace CentralLogger::Core {

LoggerLiveCardsModel::LoggerLiveCardsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LoggerLiveCardsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_sections.size());
}

QVariant LoggerLiveCardsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_sections.size()) {
        return {};
    }

    const QVariantMap &sec = m_sections.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:        return sec.value(QLatin1String(CentralLogger::Ui::kRoleName));
    case LoggerIdRole:    return sec.value(QLatin1String(CentralLogger::Ui::kRoleLoggerId));
    case StationCodeRole: return sec.value(QLatin1String(CentralLogger::Ui::kRoleStationCode));
    case HostRole:        return sec.value(QLatin1String(CentralLogger::Ui::kRoleHost));
    case EnabledRole:     return sec.value(QLatin1String(CentralLogger::Ui::kRoleEnabled));
    case OnlineRole:      return sec.value(QLatin1String(CentralLogger::Ui::kRoleOnline));
    case PollingRole:     return sec.value(QLatin1String(CentralLogger::Ui::kRolePolling));
    case AnyAlarmRole:    return sec.value(QLatin1String(CentralLogger::Ui::kRoleAnyAlarm));
    case SensorCountRole: return sec.value(QLatin1String(CentralLogger::Ui::kRoleSensorCount));
    case LastUpdateRole:  return sec.value(QLatin1String(CentralLogger::Ui::kRoleLastUpdate));
    case SensorsRole:     return sec.value(QStringLiteral("sensors"));
    default:              return {};
    }
}

QHash<int, QByteArray> LoggerLiveCardsModel::roleNames() const
{
    return {
        { Qt::DisplayRole,  CentralLogger::Ui::kRoleDisplay },
        { LoggerIdRole,     CentralLogger::Ui::kRoleLoggerId },
        { NameRole,         CentralLogger::Ui::kRoleLoggerName },
        { StationCodeRole,  CentralLogger::Ui::kRoleStationCode },
        { HostRole,         CentralLogger::Ui::kRoleHost },
        { EnabledRole,      CentralLogger::Ui::kRoleLoggerEnabled },
        { OnlineRole,       CentralLogger::Ui::kRoleOnline },
        { PollingRole,      CentralLogger::Ui::kRolePolling },
        { AnyAlarmRole,     CentralLogger::Ui::kRoleAnyAlarm },
        { SensorCountRole,  CentralLogger::Ui::kRoleSensorCount },
        { LastUpdateRole,   CentralLogger::Ui::kRoleLastUpdate },
        { SensorsRole,      QByteArrayLiteral("sensors") },
    };
}

void LoggerLiveCardsModel::setFilterText(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (m_filterText == trimmed) {
        return;
    }
    m_filterText = trimmed;
    emit filterTextChanged();
    rebuildVisible();
}

void LoggerLiveCardsModel::setSections(const QVariantList &sections)
{
    m_allSections.clear();
    m_allSections.reserve(sections.size());
    for (const QVariant &v : sections) {
        m_allSections.append(v.toMap());
    }
    rebuildVisible();
}

void LoggerLiveCardsModel::rebuildVisible()
{
    QVector<QVariantMap> next;
    if (m_filterText.isEmpty()) {
        next = m_allSections;
    } else {
        next.reserve(m_allSections.size());
        for (const QVariantMap &src : m_allSections) {
            if (matchesAnyLiveField(m_filterText, {
                    src.value(QLatin1String(CentralLogger::Ui::kRoleName)).toString(),
                    src.value(QLatin1String(CentralLogger::Ui::kRoleStationCode)).toString(),
                    src.value(QLatin1String(CentralLogger::Ui::kRoleHost)).toString() })) {
                next.append(src);
                continue;
            }
            const QVariantList sensors = src.value(QStringLiteral("sensors")).toList();
            QVariantList matchedSensors;
            for (const QVariant &sv : sensors) {
                const QVariantMap sm = sv.toMap();
                if (matchesAnyLiveField(m_filterText, {
                        sm.value(QLatin1String(CentralLogger::Ui::kRoleName)).toString(),
                        sm.value(QLatin1String(CentralLogger::Ui::kRoleUnit)).toString() })) {
                    matchedSensors.append(sm);
                }
            }
            if (!matchedSensors.isEmpty()) {
                QVariantMap sec = src;
                sec.insert(QStringLiteral("sensors"), matchedSensors);
                sec.insert(QLatin1String(CentralLogger::Ui::kRoleSensorCount),
                           int(matchedSensors.size()));
                next.append(std::move(sec));
            }
        }
    }

    bool sameIds = (next.size() == m_sections.size());
    if (sameIds) {
        for (qsizetype i = 0; i < next.size(); ++i) {
            const qint64 oldId = m_sections.at(i)
                .value(QLatin1String(CentralLogger::Ui::kRoleLoggerId)).toLongLong();
            const qint64 newId = next.at(i)
                .value(QLatin1String(CentralLogger::Ui::kRoleLoggerId)).toLongLong();
            if (oldId != newId) {
                sameIds = false;
                break;
            }
        }
    }

    if (!sameIds) {
        const int oldSize = static_cast<int>(m_sections.size());
        beginResetModel();
        m_sections = std::move(next);
        endResetModel();
        if (oldSize != m_sections.size()) {
            emit countChanged();
        }
        return;
    }

    for (int i = 0; i < next.size(); ++i) {
        if (m_sections.at(i) != next.at(i)) {
            m_sections[i] = next.at(i);
            const QModelIndex idx = index(i, 0);
            emit dataChanged(idx, idx);
        }
    }
}

} // namespace CentralLogger::Core
