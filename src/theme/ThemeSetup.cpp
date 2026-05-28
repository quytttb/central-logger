#include "ThemeSetup.h"

#include <QQuickStyle>

namespace CentralLogger::Theme {

void applyQuickControlsStyle()
{
    QQuickStyle::setStyle(QString::fromUtf8(kQuickControlsStyle));
}

} // namespace CentralLogger::Theme
