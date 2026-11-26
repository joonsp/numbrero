import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 960
    height: 680
    minimumWidth: 600
    minimumHeight: 420
    visible: true
    title: "Numbrero"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property int editorFontSize: 15
    property int layoutRev: 0
    property bool syncingText: false
    property int pendingCloseIndex: -1
    property bool pendingQuit: false
    property bool searchVisible: false
    property var searchMatches: []
    property int searchIndex: -1

    function updateSearchMatches(jumpToMatch) {
        var query = searchField.text
        if (!searchVisible || query.length === 0) {
            searchMatches = []
            searchIndex = -1
            return
        }
        var hay = editor.text.toLowerCase()
        var needle = query.toLowerCase()
        var found = []
        var idx = 0
        while (idx <= hay.length) {
            var pos = hay.indexOf(needle, idx)
            if (pos < 0)
                break
            found.push({ start: pos, end: pos + needle.length })
            idx = pos + Math.max(1, needle.length)
        }
        searchMatches = found
        if (found.length === 0) {
            searchIndex = -1
            return
        }
        if (jumpToMatch || searchIndex < 0 || searchIndex >= found.length)
            searchIndex = 0
        focusSearchMatch(searchIndex)
    }

    function focusSearchMatch(idx) {
        if (idx < 0 || idx >= searchMatches.length)
            return
        var m = searchMatches[idx]
        editor.select(m.start, m.end)
        var rect = editor.positionToRectangle(m.start)
        var top = editor.y + rect.y
        var bottom = top + rect.height
        if (top < flick.contentY + 12)
            flick.contentY = Math.max(0, top - 12)
        else if (bottom > flick.contentY + flick.height - 16)
            flick.contentY = Math.max(0, bottom - flick.height + 16)
    }

    function stepSearch(delta) {
        if (searchMatches.length === 0)
            return
        searchIndex = (searchIndex + delta + searchMatches.length) % searchMatches.length
        focusSearchMatch(searchIndex)
    }

    function lineY(charOffset, rev) {
        void(rev)
        var pos = Math.max(0, Math.min(charOffset, editor.length))
        return editor.positionToRectangle(pos).y
    }

    function lineHeight(charOffset, rev) {
        void(rev)
        var pos = Math.max(0, Math.min(charOffset, editor.length))
        return Math.max(22, editor.positionToRectangle(pos).height)
    }

    function ensureCursorVisible() {
        var rect = editor.cursorRectangle
        var top = editor.y + rect.y
        var bottom = top + rect.height
        if (top < flick.contentY + 12) {
            flick.contentY = Math.max(0, top - 12)
        } else if (bottom > flick.contentY + flick.height - 16) {
            flick.contentY = Math.max(0, bottom - flick.height + 16)
        }
    }

    function openRenameDialog(index) {
        if (index < 0 || index >= backend.sheets.length)
            return
        renameDialog.targetIndex = index
        renameField.text = backend.sheets[index].title
        renameDialog.open()
        renameField.forceActiveFocus()
        renameField.selectAll()
    }

    function requestCloseSheet(index) {
        if (index < 0 || index >= backend.sheets.length)
            return
        var sheet = backend.sheets[index]
        if (sheet.dirty) {
            win.pendingCloseIndex = index
            win.pendingQuit = false
            unsavedDialog.targetTitle = sheet.title
            unsavedDialog.open()
            return
        }
        backend.closeSheet(index)
        editor.forceActiveFocus()
    }

    function requestQuit() {
        if (backend.hasUnsavedWork) {
            win.pendingCloseIndex = -1
            win.pendingQuit = true
            unsavedDialog.targetTitle = backend.activeTitle
            unsavedDialog.open()
            return
        }
        backend.flushPendingSaves()
        Qt.quit()
    }

    onClosing: function(close) {
        if (backend.hasUnsavedWork && !win.pendingQuit) {
            close.accepted = false
            requestQuit()
        } else {
            backend.flushPendingSaves()
        }
    }

    // Application-wide shortcuts
    Shortcut { sequence: "Ctrl+Q"; context: Qt.ApplicationShortcut; onActivated: win.requestQuit() }
    Shortcut { sequence: "Ctrl+/"; context: Qt.ApplicationShortcut; onActivated: helpOverlay.visible = !helpOverlay.visible }
    Shortcut { sequence: "F1"; context: Qt.ApplicationShortcut; onActivated: helpOverlay.visible = !helpOverlay.visible }
    Shortcut { sequence: "Ctrl+T"; context: Qt.ApplicationShortcut; onActivated: { backend.newSheet(); editor.forceActiveFocus() } }
    Shortcut { sequence: "Ctrl+Shift+D"; context: Qt.ApplicationShortcut; onActivated: { backend.duplicateSheet(backend.activeIndex); editor.forceActiveFocus() } }
    Shortcut { sequence: "Ctrl+W"; context: Qt.ApplicationShortcut; onActivated: win.requestCloseSheet(backend.activeIndex) }
    Shortcut { sequence: "Ctrl+Tab"; context: Qt.ApplicationShortcut; onActivated: backend.selectSheet((backend.activeIndex + 1) % backend.sheets.length) }
    Shortcut { sequence: "Ctrl+Shift+Tab"; context: Qt.ApplicationShortcut; onActivated: backend.selectSheet((backend.activeIndex - 1 + backend.sheets.length) % backend.sheets.length) }
    Shortcut { sequence: "F2"; context: Qt.ApplicationShortcut; onActivated: win.openRenameDialog(backend.activeIndex) }
    Shortcut {
        sequence: "Ctrl+F"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!win.searchVisible && editor.selectedText.length > 0 && editor.selectedText.indexOf("\n") < 0)
                searchField.text = editor.selectedText
            win.searchVisible = true
            searchField.forceActiveFocus()
            searchField.selectAll()
            win.updateSearchMatches(true)
        }
    }
    Shortcut { sequence: "F3"; context: Qt.ApplicationShortcut; enabled: win.searchVisible; onActivated: win.stepSearch(1) }
    Shortcut { sequence: "Shift+F3"; context: Qt.ApplicationShortcut; enabled: win.searchVisible; onActivated: win.stepSearch(-1) }
    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; onActivated: backend.openFileDialog() }
    Shortcut { sequence: "Ctrl+S"; context: Qt.ApplicationShortcut; onActivated: backend.saveCurrentSheet() }
    Shortcut { sequence: "Ctrl+Shift+S"; context: Qt.ApplicationShortcut; onActivated: backend.saveFileAsDialog() }
    Shortcut { sequence: "Ctrl+Shift+C"; context: Qt.ApplicationShortcut; onActivated: backend.copyLastResult() }
    Shortcut { sequence: "Ctrl+Shift+Y"; context: Qt.ApplicationShortcut; onActivated: backend.copySheetWithResults() }
    Shortcut { sequence: "Ctrl+Shift+V"; context: Qt.ApplicationShortcut; onActivated: backend.toggleVariablesBar() }
    Shortcut { sequence: "Ctrl++"; context: Qt.ApplicationShortcut; onActivated: win.editorFontSize = Math.min(28, win.editorFontSize + 1) }
    Shortcut { sequence: "Ctrl+="; context: Qt.ApplicationShortcut; onActivated: win.editorFontSize = Math.min(28, win.editorFontSize + 1) }
    Shortcut { sequence: "Ctrl+-"; context: Qt.ApplicationShortcut; onActivated: win.editorFontSize = Math.max(11, win.editorFontSize - 1) }
    Shortcut { sequence: "Ctrl+0"; context: Qt.ApplicationShortcut; onActivated: win.editorFontSize = 15 }

    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (helpOverlay.visible) {
                helpOverlay.visible = false
                editor.forceActiveFocus()
            } else if (renameDialog.opened) {
                renameDialog.close()
                editor.forceActiveFocus()
            } else if (unsavedDialog.opened) {
                unsavedDialog.close()
                editor.forceActiveFocus()
            } else if (win.searchVisible) {
                win.searchVisible = false
                win.searchMatches = []
                win.searchIndex = -1
                editor.forceActiveFocus()
            } else if (editor.activeFocus) {
                editor.focus = false
            } else {
                editor.forceActiveFocus()
            }
        }
    }

    // Bare '?' and 'Q' are active only when the editor and dialogs do not hold focus.
    Shortcut {
        sequence: "?"
        context: Qt.ApplicationShortcut
        enabled: !editor.activeFocus && !searchField.activeFocus && !renameDialog.opened && !unsavedDialog.opened
        onActivated: helpOverlay.visible = !helpOverlay.visible
    }
    Shortcut {
        sequence: "Q"
        context: Qt.ApplicationShortcut
        enabled: !editor.activeFocus && !searchField.activeFocus && !renameDialog.opened && !unsavedDialog.opened
        onActivated: win.requestQuit()
    }

    // Direct tab switching Ctrl+1..9
    Repeater {
        model: 9
        delegate: Item {
            Shortcut {
                sequence: "Ctrl+" + (index + 1)
                context: Qt.ApplicationShortcut
                onActivated: {
                    if (index < backend.sheets.length)
                        backend.selectSheet(index)
                }
            }
        }
    }

    Connections {
        target: theme
        function onChanged() {
            backend.updateHighlighterAccent(theme.accent)
        }
    }

    Connections {
        target: backend
        function onActiveTextChanged() {
            if (editor.text !== backend.activeText) {
                win.syncingText = true
                editor.text = backend.activeText
                editor.cursorPosition = Math.min(editor.length, editor.cursorPosition)
                win.syncingText = false
                win.layoutRev += 1
            }
            if (win.searchVisible)
                win.updateSearchMatches(false)
        }
    }

    // Right-to-left languages mirror the window; Translator sets layoutDirection.
    Item {
        id: root
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Top tab & action bar
            Rectangle {
                Layout.fillWidth: true
                height: 42
                color: "#141418"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 10
                    spacing: 8

                    Image {
                        source: "qrc:/icon.svg"
                        sourceSize.width: 22
                        sourceSize.height: 22
                        width: 22
                        height: 22
                        smooth: true
                    }

                    Flickable {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentWidth: tabsRow.implicitWidth
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                        Row {
                            id: tabsRow
                            height: parent.height
                            spacing: 4

                            Repeater {
                                model: backend.sheets
                                delegate: Item {
                                    id: tabItem
                                    height: tabsRow.height
                                    width: Math.max(96, tabContent.implicitWidth + 24)

                                    readonly property bool isActive: modelData.active

                                    Rectangle {
                                        anchors.fill: parent
                                        anchors.topMargin: 5
                                        anchors.bottomMargin: 5
                                        radius: Math.min(4, theme.radius)
                                        color: isActive ? "#1D1D24" : (tabHover.hovered ? "#191920" : "transparent")
                                    }

                                    Rectangle {
                                        visible: isActive
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        height: 2
                                        color: theme.accent
                                    }

                                    HoverHandler { id: tabHover }

                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                                        onClicked: function(mouse) {
                                            if (mouse.button === Qt.MiddleButton) {
                                                win.requestCloseSheet(index)
                                            } else {
                                                backend.selectSheet(index)
                                                editor.forceActiveFocus()
                                            }
                                        }
                                        onDoubleClicked: win.openRenameDialog(index)
                                    }

                                    RowLayout {
                                        id: tabContent
                                        anchors.centerIn: parent
                                        spacing: 6

                                        Rectangle {
                                            visible: modelData.dirty
                                            width: 6
                                            height: 6
                                            radius: 3
                                            color: theme.accent
                                        }

                                        Label {
                                            text: modelData.title
                                            font.pixelSize: 13
                                            font.weight: isActive ? Font.Medium : Font.Normal
                                            color: isActive ? "#F4F4F6" : "#8E92A0"
                                            elide: Text.ElideRight
                                            Layout.maximumWidth: 160
                                        }

                                        ToolButton {
                                            visible: tabHover.hovered || isActive
                                            implicitWidth: 18
                                            implicitHeight: 18
                                            padding: 0
                                            Material.roundedScale: Math.min(4, theme.radius)
                                            onClicked: win.requestCloseSheet(index)
                                            contentItem: Label {
                                                text: "×"
                                                font.pixelSize: 13
                                                color: "#8E92A0"
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                        }
                                    }
                                }
                            }

                            ToolButton {
                                anchors.verticalCenter: parent.verticalCenter
                                implicitWidth: 28
                                implicitHeight: 28
                                padding: 0
                                Material.roundedScale: Math.min(4, theme.radius)
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("New sheet") + " (Ctrl+T)"
                                onClicked: {
                                    backend.newSheet()
                                    editor.forceActiveFocus()
                                }
                                contentItem: Label {
                                    text: "+"
                                    font.pixelSize: 17
                                    color: "#9CA3AF"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }

                    RowLayout {
                        spacing: 2

                        ToolButton {
                            implicitWidth: Math.max(44, openLabel.implicitWidth + 14)
                            implicitHeight: 30
                            Material.roundedScale: Math.min(4, theme.radius)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Open") + " (Ctrl+O)"
                            onClicked: backend.openFileDialog()
                            contentItem: Label {
                                id: openLabel
                                text: qsTr("Open")
                                font.pixelSize: 12
                                color: "#9CA3AF"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        ToolButton {
                            implicitWidth: Math.max(44, saveLabel.implicitWidth + 14)
                            implicitHeight: 30
                            Material.roundedScale: Math.min(4, theme.radius)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Save") + " (Ctrl+S)"
                            onClicked: backend.saveCurrentSheet()
                            contentItem: Label {
                                id: saveLabel
                                text: qsTr("Save")
                                font.pixelSize: 12
                                color: backend.activeDirty ? theme.accent : "#9CA3AF"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        ToolButton {
                            implicitWidth: 34
                            implicitHeight: 30
                            Material.roundedScale: Math.min(4, theme.radius)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Toggle variables bar") + " (Ctrl+Shift+V)"
                            onClicked: backend.toggleVariablesBar()
                            contentItem: Label {
                                text: "x="
                                font.family: "monospace"
                                font.pixelSize: 12
                                color: backend.variablesBarVisible ? theme.accent : "#717482"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        ToolButton {
                            implicitWidth: 28
                            implicitHeight: 30
                            Material.roundedScale: Math.min(4, theme.radius)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Shortcuts") + " (Ctrl+/)"
                            onClicked: helpOverlay.visible = !helpOverlay.visible
                            contentItem: Label {
                                text: "?"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: helpOverlay.visible ? theme.accent : "#9CA3AF"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: "#202028"
                }
            }

            // Collapsible in-sheet search bar (Ctrl+F)
            Rectangle {
                Layout.fillWidth: true
                height: 40
                visible: win.searchVisible
                color: "#141418"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 8

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 320
                        height: 28
                        radius: Math.min(4, theme.radius)
                        color: "#1B1B22"
                        border.width: 1
                        border.color: searchField.activeFocus ? theme.accent : "#2A2A35"

                        Label {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            verticalAlignment: Text.AlignVCenter
                            text: qsTr("Find in sheet")
                            font.pixelSize: 13
                            color: "#717482"
                            visible: searchField.text.length === 0
                            elide: Text.ElideRight
                        }

                        TextInput {
                            id: searchField
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            verticalAlignment: TextInput.AlignVCenter
                            font.pixelSize: 13
                            color: "#F4F4F6"
                            selectionColor: Qt.rgba(Qt.color(theme.accent).r, Qt.color(theme.accent).g, Qt.color(theme.accent).b, 0.3)
                            selectedTextColor: "#FFFFFF"
                            selectByMouse: true
                            clip: true
                            onTextChanged: win.updateSearchMatches(true)
                            Keys.onReturnPressed: function(event) {
                                win.stepSearch((event.modifiers & Qt.ShiftModifier) ? -1 : 1)
                            }
                            Keys.onEnterPressed: function(event) {
                                win.stepSearch((event.modifiers & Qt.ShiftModifier) ? -1 : 1)
                            }
                        }
                    }

                    Label {
                        font.family: "monospace"
                        font.pixelSize: 12
                        color: win.searchMatches.length > 0 ? "#D4D4DC" : "#717482"
                        text: win.searchMatches.length > 0
                            ? (win.searchIndex + 1) + "/" + win.searchMatches.length
                            : (searchField.text.length > 0 ? "0" : "")
                    }

                    ToolButton {
                        implicitWidth: 26
                        implicitHeight: 26
                        enabled: win.searchMatches.length > 0
                        Material.roundedScale: Math.min(4, theme.radius)
                        onClicked: win.stepSearch(-1)
                        contentItem: Label {
                            text: "↑"
                            font.pixelSize: 13
                            color: "#9CA3AF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    ToolButton {
                        implicitWidth: 26
                        implicitHeight: 26
                        enabled: win.searchMatches.length > 0
                        Material.roundedScale: Math.min(4, theme.radius)
                        onClicked: win.stepSearch(1)
                        contentItem: Label {
                            text: "↓"
                            font.pixelSize: 13
                            color: "#9CA3AF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Item { Layout.fillWidth: true }

                    ToolButton {
                        implicitWidth: 26
                        implicitHeight: 26
                        Material.roundedScale: Math.min(4, theme.radius)
                        onClicked: {
                            win.searchVisible = false
                            win.searchMatches = []
                            win.searchIndex = -1
                            editor.forceActiveFocus()
                        }
                        contentItem: Label {
                            text: "×"
                            font.pixelSize: 14
                            color: "#9CA3AF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: "#202028"
                }
            }

            // Main REPL editor + live results gutter
            Flickable {
                id: flick
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: width
                contentHeight: Math.max(height, editor.contentHeight + 48)
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                readonly property int gutterWidth: Math.max(175, Math.min(285, Math.round(width * 0.34)))

                // Active cursor line highlight across both panes
                Rectangle {
                    x: 0
                    y: editor.y + editor.cursorRectangle.y
                    width: flick.width
                    height: Math.max(22, editor.cursorRectangle.height)
                    color: "#15151B"
                    visible: editor.activeFocus
                }

                // Vertical separator between sheet and results gutter
                Rectangle {
                    x: flick.width - flick.gutterWidth
                    y: 0
                    width: 1
                    height: flick.contentHeight
                    color: "#1F1F27"
                }

                // Left pane: plain-text math sheet
                TextEdit {
                    id: editor
                    x: 20
                    y: 16
                    width: Math.max(160, flick.width - flick.gutterWidth - 36)
                    height: Math.max(flick.height - 32, contentHeight)
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    persistentSelection: true
                    focus: true
                    color: "#E8E8EC"
                    selectionColor: Qt.rgba(Qt.color(theme.accent).r, Qt.color(theme.accent).g, Qt.color(theme.accent).b, 0.25)
                    selectedTextColor: "#FFFFFF"
                    font.family: "JetBrainsMono Nerd Font, CaskaydiaMono Nerd Font, Adwaita Mono, monospace"
                    font.pixelSize: win.editorFontSize

                    Component.onCompleted: {
                        win.syncingText = true
                        text = backend.activeText
                        win.syncingText = false
                        backend.attachHighlighter(textDocument, theme.accent)
                        win.layoutRev += 1
                        forceActiveFocus()
                    }

                    onTextChanged: {
                        if (!win.syncingText)
                            backend.setActiveText(text)
                        win.layoutRev += 1
                    }
                    onWidthChanged: win.layoutRev += 1
                    onContentHeightChanged: win.layoutRev += 1
                    onCursorRectangleChanged: win.ensureCursorVisible()
                }

                // Click empty space below text to focus editor at end
                MouseArea {
                    x: 0
                    y: editor.y + editor.contentHeight
                    width: flick.width - flick.gutterWidth
                    height: Math.max(0, flick.contentHeight - y)
                    onClicked: {
                        editor.cursorPosition = editor.length
                        editor.forceActiveFocus()
                    }
                }

                // Right pane: aligned live REPL results
                Item {
                    id: gutter
                    x: flick.width - flick.gutterWidth + 1
                    y: editor.y
                    width: flick.gutterWidth - 1
                    height: editor.height

                    Repeater {
                        model: backend.results
                        delegate: Item {
                            id: resultRow
                            x: 0
                            y: win.lineY(modelData.charOffset, win.layoutRev)
                            width: gutter.width
                            height: win.lineHeight(modelData.charOffset, win.layoutRev)

                            readonly property bool hasValue: modelData.displayText.length > 0
                            readonly property bool hasError: modelData.errorText.length > 0

                            Rectangle {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 12
                                anchors.topMargin: 1
                                anchors.bottomMargin: 1
                                radius: Math.min(4, theme.radius)
                                color: rowHover.hovered && resultRow.hasValue ? "#1C1C24" : "transparent"
                            }

                            Label {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.leftMargin: 14
                                anchors.rightMargin: 18
                                anchors.verticalCenter: parent.verticalCenter
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                                font.family: editor.font.family
                                font.pixelSize: Math.max(12, win.editorFontSize - 1)
                                font.weight: modelData.kind === "expression" ? Font.Medium : Font.Normal
                                text: resultRow.hasError ? modelData.errorText : modelData.displayText
                                color: resultRow.hasError
                                    ? "#F87171"
                                    : (modelData.kind === "assignment"
                                        ? Qt.rgba(Qt.color(theme.accent).r, Qt.color(theme.accent).g, Qt.color(theme.accent).b, 0.85)
                                        : theme.accent)
                                ToolTip.visible: rowHover.hovered && resultRow.hasError
                                ToolTip.text: modelData.errorText
                            }

                            HoverHandler { id: rowHover }

                            MouseArea {
                                anchors.fill: parent
                                enabled: resultRow.hasValue
                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: backend.copyToClipboard(modelData.copyText)
                            }
                        }
                    }
                }
            }

            // Collapsible live variables strip
            Rectangle {
                Layout.fillWidth: true
                height: 38
                visible: backend.variablesBarVisible && backend.variableCount > 0
                color: "#121216"

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 1
                    color: "#1F1F27"
                }

                Flickable {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    contentWidth: varRow.implicitWidth
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    Row {
                        id: varRow
                        height: parent.height
                        spacing: 8

                        Repeater {
                            model: backend.variables
                            delegate: Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                height: 24
                                width: chipRow.implicitWidth + 16
                                radius: Math.min(4, theme.radius)
                                color: chipHover.hovered ? "#22222B" : "#18181F"
                                border.color: "#262631"
                                border.width: 1

                                HoverHandler { id: chipHover }

                                Row {
                                    id: chipRow
                                    anchors.centerIn: parent
                                    spacing: 6

                                    Label {
                                        text: modelData.name
                                        font.family: editor.font.family
                                        font.pixelSize: 12
                                        color: "#9CA3AF"
                                    }

                                    Label {
                                        text: modelData.displayText
                                        font.family: editor.font.family
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: theme.accent
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: function(mouse) {
                                        if (mouse.button === Qt.RightButton) {
                                            editor.insert(editor.cursorPosition, modelData.name)
                                            editor.forceActiveFocus()
                                        } else {
                                            backend.copyToClipboard(modelData.copyText)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Bottom status bar
            Rectangle {
                Layout.fillWidth: true
                height: 28
                color: "#101014"

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 1
                    color: "#1D1D24"
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 12

                    Label {
                        readonly property string lang: translator.language
                        text: {
                            void(lang)
                            if (backend.statusNotice.length > 0)
                                return backend.statusNotice
                            if (backend.activeFilePath.length === 0)
                                return qsTr("Auto-saved")
                            if (backend.activeDirty)
                                return qsTr("Unsaved") + " · " + backend.activeFilePath
                            return qsTr("Saved") + " · " + backend.activeFilePath
                        }
                        font.pixelSize: 11
                        color: backend.statusNotice.length > 0 ? theme.accent : (backend.activeDirty ? "#FBBF24" : "#717482")
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }

                    Label {
                        readonly property string lang: translator.language
                        text: {
                            void(lang)
                            return qsTr("%n lines", "", backend.lineCount) + "  ·  " +
                                   qsTr("%n variables", "", backend.variableCount)
                        }
                        font.pixelSize: 11
                        color: "#717482"
                    }

                    ToolButton {
                        implicitWidth: Math.max(30, precLabel.implicitWidth + 10)
                        implicitHeight: 22
                        padding: 0
                        Material.roundedScale: Math.min(4, theme.radius)
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Decimals: %1").arg(backend.precision)
                        onClicked: backend.cyclePrecision()
                        contentItem: Label {
                            id: precLabel
                            text: "." + backend.precision
                            font.family: "monospace"
                            font.pixelSize: 11
                            color: "#8E92A0"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
        }

        ShortcutOverlay {
            id: helpOverlay
            onClosed: {
                helpOverlay.visible = false
                editor.forceActiveFocus()
            }
        }
    }

    Dialog {
        id: renameDialog
        title: qsTr("Rename Sheet")
        modal: true
        anchors.centerIn: parent
        width: 340
        Material.roundedScale: theme.radius
        standardButtons: Dialog.Ok | Dialog.Cancel

        property int targetIndex: -1

        contentItem: TextField {
            id: renameField
            placeholderText: qsTr("Sheet name")
            selectByMouse: true
            onAccepted: renameDialog.accept()
        }

        onAccepted: {
            backend.renameSheet(targetIndex, renameField.text)
            editor.forceActiveFocus()
        }
        onRejected: editor.forceActiveFocus()
    }

    Dialog {
        id: unsavedDialog
        title: qsTr("Unsaved Changes")
        modal: true
        anchors.centerIn: parent
        width: 380
        Material.roundedScale: theme.radius

        property string targetTitle: ""

        contentItem: Label {
            text: qsTr("Save changes to %1 before closing?").arg(unsavedDialog.targetTitle)
            wrapMode: Text.WordWrap
            color: "#D4D4DC"
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Discard")
                flat: true
                Material.roundedScale: Math.min(4, theme.radius)
                onClicked: {
                    unsavedDialog.close()
                    if (win.pendingQuit) {
                        Qt.quit()
                    } else if (win.pendingCloseIndex >= 0) {
                        backend.closeSheet(win.pendingCloseIndex)
                        win.pendingCloseIndex = -1
                        editor.forceActiveFocus()
                    }
                }
            }
            Button {
                text: qsTr("Cancel")
                flat: true
                Material.roundedScale: Math.min(4, theme.radius)
                onClicked: {
                    win.pendingQuit = false
                    win.pendingCloseIndex = -1
                    unsavedDialog.close()
                    editor.forceActiveFocus()
                }
            }
            Button {
                text: qsTr("Save")
                highlighted: true
                Material.roundedScale: Math.min(4, theme.radius)
                onClicked: {
                    if (win.pendingCloseIndex >= 0 && win.pendingCloseIndex !== backend.activeIndex)
                        backend.selectSheet(win.pendingCloseIndex)
                    if (backend.saveCurrentSheet()) {
                        unsavedDialog.close()
                        if (win.pendingQuit) {
                            Qt.quit()
                        } else if (win.pendingCloseIndex >= 0) {
                            backend.closeSheet(win.pendingCloseIndex)
                            win.pendingCloseIndex = -1
                            editor.forceActiveFocus()
                        }
                    }
                }
            }
        }
    }
}
