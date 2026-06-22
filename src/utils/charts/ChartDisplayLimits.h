#pragma once

namespace CentralLogger::Utils {

/// Points shown on Dashboard ingestion chart and Logger Detail trending chart.
inline constexpr int kChartDisplayPointCount = 20;

/// Dashboard readings chart SQL refresh interval (wall clock).
inline constexpr int kReadingsChartRefreshMs = 30'000;

} // namespace CentralLogger::Utils
