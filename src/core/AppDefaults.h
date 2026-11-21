#pragma once

// QML-facing wrapper around CentralLogger::Defaults + UI constants.
// Centralizes ports, intervals, and theme tokens so QML views
// (LoggersSettingsPane, ThemeToggle, ChartTimeSeriesPanel) match C++ defaults
// without hardcoding literals.
//
// Registered as a QML_SINGLETON under CentralLogger.Core.AppDefaults.

#include "utils/AppConstants.h"
#include "utils/UiConstants.h"

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

namespace CentralLogger::Core {

class AppDefaults : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // --- Network defaults ----------------------------------------------------
    Q_PROPERTY(int modbusPort   READ modbusPort   CONSTANT)
    Q_PROPERTY(int apiPort      READ apiPort      CONSTANT)
    Q_PROPERTY(int modbusUnitId READ modbusUnitId CONSTANT)

    // --- Polling / timing defaults ------------------------------------------
    Q_PROPERTY(int pollIntervalSec READ pollIntervalSec CONSTANT)
    Q_PROPERTY(int timeoutSec      READ timeoutSec      CONSTANT)
    Q_PROPERTY(int minIntervalSec  READ minIntervalSec  CONSTANT)
    Q_PROPERTY(int maxIntervalSec  READ maxIntervalSec  CONSTANT)

    // --- Charts --------------------------------------------------------------
    Q_PROPERTY(int readingsChartRefreshMs READ readingsChartRefreshMs CONSTANT)

    // --- Responsive layout --------------------------------------------------
    Q_PROPERTY(int widePaneWidth READ widePaneWidth CONSTANT)
    Q_PROPERTY(int wideToolbarWidth READ wideToolbarWidth CONSTANT)
    Q_PROPERTY(int wideLiveHeaderWidth READ wideLiveHeaderWidth CONSTANT)

    // --- Theme defaults ------------------------------------------------------
    Q_PROPERTY(QString themeDark  READ themeDark  CONSTANT)
    Q_PROPERTY(QString themeLight READ themeLight CONSTANT)

public:
    explicit AppDefaults(QObject *parent = nullptr);

    int     modbusPort()      const { return Defaults::kDefaultModbusPort; }
    int     apiPort()         const { return Defaults::kDefaultApiPort; }
    int     modbusUnitId()    const { return Defaults::kDefaultModbusUnitId; }
    int     pollIntervalSec() const { return Defaults::kDefaultPollIntervalSec; }
    int     timeoutSec()      const { return Defaults::kDefaultTimeoutSec; }
    int     minIntervalSec()  const { return Defaults::kMinIntervalSec; }
    int     maxIntervalSec()  const { return Defaults::kMaxIntervalSec; }

    int     readingsChartRefreshMs() const { return Defaults::kReadingsChartRefreshMs; }

    int widePaneWidth() const { return Ui::kWidePaneWidth; }
    int wideToolbarWidth() const { return Ui::kWideToolbarWidth; }
    int wideLiveHeaderWidth() const { return Ui::kWideLiveHeaderWidth; }

    QString themeDark()  const { return QString::fromUtf8(Ui::kThemeDark); }
    QString themeLight() const { return QString::fromUtf8(Ui::kThemeLight); }
};

} // namespace CentralLogger::Core
