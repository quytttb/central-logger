import QtQuick

// Data area: children when hasData; otherwise EmptyStatePlaceholder only.
Item {
    id: root

    property bool   hasData: true
    property string emptyIconName: "informationOutline"
    property string emptyMessage: ""

    default property alias dataContent: dataLayer.data

    Item {
        id: dataLayer
        anchors.fill: parent
        visible: root.hasData
    }

    EmptyStatePlaceholder {
        anchors.fill: parent
        visible: !root.hasData
        iconName: root.emptyIconName
        message: root.emptyMessage
    }
}
