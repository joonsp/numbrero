import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: overlay
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.72)
    visible: false
    z: 100

    signal closed()

    MouseArea {
        anchors.fill: parent
        onClicked: overlay.closed()
    }

    Rectangle {
        id: card
        width: Math.min(parent.width - 48, 720)
        height: Math.min(parent.height - 48, contentCol.implicitHeight + 48)
        anchors.centerIn: parent
        color: "#151519"
        border.color: "#2A2A33"
        border.width: 1
        radius: theme.radius

        MouseArea {
            anchors.fill: parent
        }

        ColumnLayout {
            id: contentCol
            anchors.fill: parent
            anchors.margins: 24
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: qsTr("Keyboard Shortcuts & Syntax")
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: "#F4F4F6"
                    Layout.fillWidth: true
                }
                Button {
                    text: qsTr("Close")
                    flat: true
                    Material.roundedScale: Math.min(4, theme.radius)
                    onClicked: overlay.closed()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#25252E"
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 28
                rowSpacing: 14

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: 8

                    Label {
                        text: qsTr("Shortcuts")
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: theme.accent
                    }

                    Repeater {
                        model: [
                            { keys: "Ctrl+T", desc: qsTr("New sheet") },
                            { keys: "Ctrl+Shift+D", desc: qsTr("Duplicate sheet") },
                            { keys: "Ctrl+W", desc: qsTr("Close sheet") },
                            { keys: "Ctrl+Tab", desc: qsTr("Next sheet") },
                            { keys: "F2", desc: qsTr("Rename sheet") },
                            { keys: "Ctrl+F", desc: qsTr("Find in sheet") },
                            { keys: "Ctrl+O / Ctrl+S", desc: qsTr("Open / Save file") },
                            { keys: "Ctrl+Shift+C", desc: qsTr("Copy last result") },
                            { keys: "Ctrl+Shift+Y", desc: qsTr("Copy sheet with results") },
                            { keys: "Ctrl+Shift+V", desc: qsTr("Toggle variables bar") },
                            { keys: "Ctrl++ / Ctrl+- / Ctrl+0", desc: qsTr("Zoom text") },
                            { keys: "Ctrl+/  ·  F1", desc: qsTr("Toggle help") },
                            { keys: "Ctrl+Q", desc: qsTr("Quit") }
                        ]
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Label {
                                text: modelData.keys
                                font.family: "monospace"
                                font.pixelSize: 12
                                color: "#D4D4DC"
                                Layout.preferredWidth: 185
                                elide: Text.ElideRight
                            }
                            Label {
                                text: modelData.desc
                                font.pixelSize: 12
                                color: "#9CA3AF"
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: 8

                    Label {
                        text: qsTr("Expressions")
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: theme.accent
                    }

                    Repeater {
                        model: [
                            { ex: "apples = 3", note: qsTr("Variable") },
                            { ex: "2(3 + 4) · 2pi", note: qsTr("Implicit multiply") },
                            { ex: "20% of 150", note: qsTr("Percentage of") },
                            { ex: "20% off $150", note: qsTr("Currency & discount") },
                            { ex: "5 km in mi", note: qsTr("Unit conversion") },
                            { ex: "72 F in C", note: qsTr("Temperature") },
                            { ex: "255 in hex", note: qsTr("Hex / bin / oct") },
                            { ex: "sum · avg · prev", note: qsTr("Block aggregates") },
                            { ex: "sqrt(16) + pi", note: qsTr("Math & constants") },
                            { ex: "# Heading  // note", note: qsTr("Sections & comments") }
                        ]
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Label {
                                text: modelData.ex
                                font.family: "monospace"
                                font.pixelSize: 12
                                color: "#E8E8EC"
                                Layout.preferredWidth: 155
                                elide: Text.ElideRight
                            }
                            Label {
                                text: modelData.note
                                font.pixelSize: 12
                                color: "#9CA3AF"
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
    }
}
