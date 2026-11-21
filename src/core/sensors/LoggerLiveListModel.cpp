#include "core/sensors/LoggerLiveListModel.h"

#include "core/sensors/SensorLiveRow.h"
#include "utils/SensorConstants.h"
#include "utils/UiConstants.h"

namespace CentralLogger::Core {

LoggerLiveListModel::LoggerLiveListModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int LoggerLiveListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_visibleEntries.size());
}

int LoggerLiveListModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(ColumnCount);
}

QVariant LoggerLiveListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visibleEntries.size()) {
        return {};
    }
    const LoggerLiveListEntry &e = m_visibleEntries[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case LoggerColumn: return e.loggerName;
        case SensorColumn: return e.sensorName;
        case ValueColumn:  return e.valid ? e.value
                                          : QString::fromUtf8(CentralLogger::Sensor::kValuePlaceholder);
        case UnitColumn:   return e.unit;
        case StatusColumn: return e.displayStatus;
        case LimitColumn:  return e.hasLimit
                                  ? e.limitText
                                  : QString::fromUtf8(CentralLogger::Sensor::kValuePlaceholder);
        default:           return {};
        }
    }

    switch (role) {
    case LoggerIdRole:      return QVariant::fromValue(e.loggerId);
    case LoggerNameRole:    return e.loggerName;
    case StationCodeRole:   return e.stationCode;
    case OnlineRole:        return e.online;
    case EdgeSensorIdRole:  return e.edgeSensorId;
    case NameRole:          return e.sensorName;
    case SensorTypeRole:    return e.sensorType;
    case ValueRole:         return e.valid ? e.value
                                           : QString::fromUtf8(CentralLogger::Sensor::kValuePlaceholder);
    case UnitRole:          return e.unit;
    case DisplayStatusRole: return e.displayStatus;
    case Status03AutoRole:  return e.status03Auto;
    case AlarmTypeRole:     return e.alarmType;
    case AttachDiCodesRole: return e.attachDiTypeCodes;
    case AttachDiLabelsRole:return e.attachDiTypeLabels;
    case HasLimitRole:      return e.hasLimit;
    case LimitTextRole:     return e.limitText;
    case ValidRole:         return e.valid;
    default:                return {};
    }
}

QVariant LoggerLiveListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }
    switch (section) {
    case LoggerColumn: return tr("Logger");
    case SensorColumn: return tr("Sensor");
    case ValueColumn:  return tr("Value");
    case UnitColumn:   return tr("Unit");
    case StatusColumn: return tr("Status");
    case LimitColumn:  return tr("Limit");
    default:           return {};
    }
}

QHash<int, QByteArray> LoggerLiveListModel::roleNames() const
{
    return {
        { Qt::DisplayRole,   CentralLogger::Ui::kRoleDisplay },
        { LoggerIdRole,      CentralLogger::Ui::kRoleLoggerId },
        { LoggerNameRole,    CentralLogger::Ui::kRoleLoggerName },
        { StationCodeRole,   CentralLogger::Ui::kRoleStationCode },
        { OnlineRole,        CentralLogger::Ui::kRoleOnline },
        { EdgeSensorIdRole,  CentralLogger::Ui::kRoleEdgeSensorId },
        { NameRole,          CentralLogger::Ui::kRoleName },
        { SensorTypeRole,    CentralLogger::Ui::kRoleSensorType },
        { ValueRole,         CentralLogger::Ui::kRoleValue },
        { UnitRole,          CentralLogger::Ui::kRoleUnit },
        { DisplayStatusRole, CentralLogger::Ui::kRoleDisplayStatus },
        { Status03AutoRole,  CentralLogger::Ui::kRoleStatus03Auto },
        { AlarmTypeRole,     CentralLogger::Ui::kRoleAlarmType },
        { AttachDiCodesRole, CentralLogger::Ui::kRoleAttachDiCodes },
        { AttachDiLabelsRole,CentralLogger::Ui::kRoleAttachDiLabels },
        { HasLimitRole,      CentralLogger::Ui::kRoleHasLimit },
        { LimitTextRole,     CentralLogger::Ui::kRoleLimitText },
        { ValidRole,         CentralLogger::Ui::kRoleValid },
    };
}

void LoggerLiveListModel::setFilterText(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (m_filterText == trimmed) {
        return;
    }
    m_filterText = trimmed;
    emit filterTextChanged();
    rebuildVisible();
}

void LoggerLiveListModel::setEntries(QVector<LoggerLiveListEntry> entries)
{
    m_allEntries = std::move(entries);
    rebuildVisible();
}

void LoggerLiveListModel::rebuildVisible()
{
    QVector<LoggerLiveListEntry> next;
    next.reserve(m_allEntries.size());
    for (const auto &entry : m_allEntries) {
        if (matchesAnyLiveField(m_filterText, {
                entry.loggerName, entry.stationCode, entry.host,
                entry.sensorName, entry.unit })) {
            next.append(entry);
        }
    }

    bool sameLayout = (next.size() == m_visibleEntries.size());
    if (sameLayout) {
        for (int i = 0; i < next.size(); ++i) {
            if (!next[i].sameIdentity(m_visibleEntries[i])) {
                sameLayout = false;
                break;
            }
        }
    }

    if (!sameLayout) {
        const bool countDiff = (next.size() != m_visibleEntries.size());
        beginResetModel();
        m_visibleEntries = std::move(next);
        endResetModel();
        if (countDiff) {
            emit countChanged();
        }
        return;
    }

    for (int i = 0; i < next.size(); ++i) {
        if (!(next[i] == m_visibleEntries[i])) {
            m_visibleEntries[i] = next[i];
            emit dataChanged(index(i, 0), index(i, ColumnCount - 1));
        }
    }
}

} // namespace CentralLogger::Core
