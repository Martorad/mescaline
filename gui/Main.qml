import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1700
    height: 850
    minimumWidth: 1000
    minimumHeight: 550
    visible: true
    title: "Mescaline"
    // Numeric entry uses a dot regardless of the desktop locale.
    function normalizeFraction(field) {
        const normalized = field.text.replace(/,/g, ".")
        if (normalized !== field.text) {
            const cursor = field.cursorPosition
            field.text = normalized
            field.cursorPosition = cursor
        }
    }

    function normalizeExpression(field) {
        // ponytail: rescan nested calls on edit; tokenize once if long expressions lag.
        const input = field.text
        const functions = []
        let normalized = ""
        for (let i = 0; i < input.length; ++i) {
            let character = input[i]
            if (character === "(") {
                const name = input.slice(0, i).match(/[A-Za-z_][A-Za-z_0-9]*\s*$/)
                let depth = 0
                let semicolon = false
                for (let j = i + 1; j < input.length; ++j) {
                    if (input[j] === "(") ++depth
                    else if (input[j] === ")") {
                        if (depth === 0) break
                        --depth
                    } else if (input[j] === ";" && depth === 0) semicolon = true
                }
                functions.push({ name: name ? name[0].trim() : "", semicolon: semicolon })
            } else if (character === ")") {
                functions.pop()
            } else if (character === "," && /[0-9]/.test(input[i + 1] || "") &&
                       (i === 0 || /[0-9\s(+\-*/^]/.test(input[i - 1])) &&
                       (!functions.length || !["min", "max", "pow"].includes(functions[functions.length - 1].name) ||
                        functions[functions.length - 1].semicolon)) {
                character = "."
            }
            normalized += character
        }
        if (normalized !== input) {
            const cursor = field.cursorPosition
            field.text = normalized
            field.cursorPosition = cursor
        }
    }

    property bool ready: false
    property bool pendingPreview: false
    property string previewKey: argumentsFor(true).join("\u0000")
    onPreviewKeyChanged: if (ready && livePreview.checked) liveRefresh.restart()
    Component.onCompleted: ready = true
    onClosing: function(close) {
        if (renderer.running) {
            renderer.cancel()
            close.accepted = false
        }
    }
    Material.theme: darkTheme.checked ? Material.Dark
                    : lightTheme.checked ? Material.Light
                    : (renderer.systemDark ? Material.Dark : Material.Light)

    ActionGroup { id: themeChoices }
    Action { id: systemTheme; text: "System"; checkable: true; checked: true; ActionGroup.group: themeChoices }
    Action { id: lightTheme; text: "Light"; checkable: true; ActionGroup.group: themeChoices }
    Action { id: darkTheme; text: "Dark"; checkable: true; ActionGroup.group: themeChoices }
    Action { id: reducedPreview; objectName: "reducedPreview"; text: "Reduced preview resolution"; checkable: true }

    Dialog {
        id: settingsDialog
        objectName: "settingsDialog"
        parent: Overlay.overlay
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        width: 760
        height: 480
        title: "Settings"
        modal: true
        focus: true
        standardButtons: Dialog.Close
        property int category: 0

        RowLayout {
            anchors.fill: parent
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Label { text: settingsDialog.category === 0 ? "Appearance" : "Performance"; font.pixelSize: 20; font.bold: true }
                ColumnLayout {
                    objectName: "appearancePane"
                    visible: settingsDialog.category === 0
                    Layout.fillWidth: true
                    Label { text: "Theme" }
                    ComboBox {
                        objectName: "themeSetting"
                        Layout.fillWidth: true
                        model: ["System", "Light", "Dark"]
                        currentIndex: darkTheme.checked ? 2 : lightTheme.checked ? 1 : 0
                        onActivated: function(index) {
                            if (index === 0) systemTheme.checked = true
                            else if (index === 1) lightTheme.checked = true
                            else darkTheme.checked = true
                        }
                        Accessible.name: "Theme"
                    }
                }
                ColumnLayout {
                    objectName: "performancePane"
                    visible: settingsDialog.category === 1
                    Layout.fillWidth: true
                    Switch {
                        text: "Reduce preview resolution"
                        checked: reducedPreview.checked
                        onToggled: reducedPreview.checked = checked
                        Accessible.name: text
                    }
                    Label { text: "Preview scale (%)" }
                    TextField {
                        id: previewScale
                        objectName: "previewScale"
                        Layout.fillWidth: true
                        text: "50"
                        validator: RegularExpressionValidator {
                            regularExpression: /(?:100(?:[.,]0*)?|[1-9]?[0-9](?:[.,][0-9]*)?)/
                        }
                        onTextChanged: window.normalizeFraction(previewScale)
                        onEditingFinished: {
                            if (text.length === 0 || Number(text) < 1 || Number(text) > 100) text = "50"
                        }
                        enabled: reducedPreview.checked
                        Accessible.name: "Preview scale percent"
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "Only previews are scaled. Export dimensions stay unchanged."
                    }
                    Label { text: "Threads (0 = auto)" }
                    SpinBox {
                        id: threads
                        objectName: "threadsSetting"
                        from: 0
                        to: 1024
                        value: 0
                        editable: true
                        Layout.fillWidth: true
                        Accessible.name: "Worker threads"
                    }
                }
                Item { Layout.fillHeight: true }
            }
            Frame {
                objectName: "settingsCategories"
                Layout.preferredWidth: 190
                Layout.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent
                    Button {
                        text: "Appearance"
                        Layout.fillWidth: true
                        highlighted: settingsDialog.category === 0
                        onClicked: settingsDialog.category = 0
                        Accessible.name: "Appearance settings"
                    }
                    Button {
                        text: "Performance"
                        Layout.fillWidth: true
                        highlighted: settingsDialog.category === 1
                        onClicked: settingsDialog.category = 1
                        Accessible.name: "Performance settings"
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }

    Dialog {
        id: expressionHelp
        objectName: "expressionHelp"
        parent: Overlay.overlay
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        width: Math.min(900, window.width - 32)
        height: Math.min(650, window.height - 32)
        title: "Scalar expression cheat sheet"
        modal: true
        focus: true
        standardButtons: Dialog.Close

        ScrollView {
            id: helpScroll
            anchors.fill: parent
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: helpScroll.availableWidth
                spacing: 0
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Item"; font.bold: true; Layout.preferredWidth: 300 }
                    Label { text: "Meaning"; font.bold: true; Layout.fillWidth: true }
                }
                Repeater {
                    id: helpRows
                    objectName: "expressionRows"
                    model: [
                        { section: "Coordinates and constants", item: "x", meaning: "Horizontal position from 0 to 1." },
                        { item: "y", meaning: "Vertical position from 0 to 1." },
                        { item: "px", meaning: "Zero-based pixel column, from the left." },
                        { item: "py", meaning: "Zero-based pixel row, from the top." },
                        { item: "width", meaning: "Canvas width in pixels." },
                        { item: "height", meaning: "Canvas height in pixels." },
                        { item: "pi", meaning: "3.14159…" },
                        { item: "e", meaning: "2.71828…" },
                        { section: "Operators", item: "+  -  *  /", meaning: "Add, subtract, multiply, divide." },
                        { item: "%", meaning: "Remainder." },
                        { item: "^", meaning: "Power." },
                        { item: "( )", meaning: "Group an expression." },
                        { item: "+a  -a", meaning: "Unary positive or negative." },
                        { section: "Functions", item: "sin(a)", meaning: "Sine; angle in radians." },
                        { item: "cos(a)", meaning: "Cosine; angle in radians." },
                        { item: "tan(a)", meaning: "Tangent; angle in radians." },
                        { item: "sqrt(a)", meaning: "Square root." },
                        { item: "log(a)", meaning: "Natural logarithm." },
                        { item: "abs(a)", meaning: "Absolute value." },
                        { item: "min(a,b)", meaning: "Smaller of two values." },
                        { item: "max(a,b)", meaning: "Larger of two values." },
                        { item: "pow(a,b)", meaning: "a raised to the power b." },
                        { section: "Animation and randomness", item: "frame", meaning: "Zero-based frame number." },
                        { item: "t", meaning: "frame / total frames; starts at 0, ends below 1." },
                        { item: "random()", meaning: "Repeatable value from 0 to 1 for each pixel and frame." },
                        { item: "seed", meaning: "Seed setting as a variable; changing Seed changes random()." },
                        { section: "Examples", item: "x", meaning: "Horizontal gradient." },
                        { item: "sin(x * pi * 8) * 0.5 + 0.5", meaning: "Stripes." },
                        { item: "sin((x + t) * pi * 8) * 0.5 + 0.5", meaning: "Moving stripes; set Frames above 1 and export GIF or frames." },
                        { item: "random()", meaning: "Seeded grain." },
                        { section: "Output and input", item: "0 to 1", meaning: "Expression values span the selected palette; Range mode wraps or clamps values outside the range." },
                        { item: "min(0,5; 1,5)", meaning: "Decimal commas become periods; use ; between arguments when entering commas." }
                    ]
                    delegate: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Label {
                            visible: modelData.section !== undefined
                            text: modelData.section || ""
                            font.bold: true
                            Layout.topMargin: 12
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            Label {
                                text: modelData.item
                                font.family: "monospace"
                                Layout.preferredWidth: 288
                                wrapMode: Text.WrapAnywhere
                            }
                            Label {
                                text: modelData.meaning
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }
                        Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(0.5, 0.5, 0.5, 0.25) }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: 12
                    wrapMode: Text.WordWrap
                    text: "Preview shows only the first frame. Decimals are displayed with periods (e.g. min(0.5; 1.5))."
                }
            }
        }
    }

    function argumentsFor(preview) {
        const args = []
        if (mode.currentIndex === 0) {
            args.push("--algorithm=" + algorithm.currentText)
            args.push("--scale=" + scale.text)
            args.push("--color=" + color.text)
        } else if (mode.currentIndex === 1) {
            args.push("--expression=" + scalar.text)
            args.push("--palette=" + palette.currentText)
            if (palette.currentText === "monochrome") args.push("--color=" + color.text)
            args.push("--seed=" + seed.text)
        } else {
            args.push("--expression-r=" + red.text)
            args.push("--expression-g=" + green.text)
            args.push("--expression-b=" + blue.text)
            args.push("--seed=" + seed.text)
        }
        const percent = Number(previewScale.text)
        const factor = preview && reducedPreview.checked
                       ? (Number.isFinite(percent) && percent >= 1 ? Math.min(percent, 100) : 50) / 100
                       : 1
        args.push("--width=" + Math.max(1, Math.round(canvasWidth.value * factor)))
        args.push("--height=" + Math.max(1, Math.round(canvasHeight.value * factor)))
        args.push("--range-mode=" + rangeMode.currentText)
        args.push("--threads=" + threads.value)
        if (!preview) {
            args.push("--frames=" + frames.value)
            args.push("--fps=" + fps.value)
            if (force.checked) args.push("--force")
        }
        return args
    }

    function launch(preview) {
        renderer.start(argumentsFor(preview), output.text.trim(), preview, preview && livePreview.checked)
    }

    Timer {
        id: liveRefresh
        interval: 400
        onTriggered: {
            if (!livePreview.checked) return
            if (renderer.running) {
                window.pendingPreview = true
                if (renderer.previewRunning) renderer.cancel()
            } else {
                window.pendingPreview = false
                window.launch(true)
            }
        }
    }

    Connections {
        target: renderer
        function onChanged() {
            if (!window.pendingPreview || renderer.running || liveRefresh.running || !livePreview.checked) return
            window.pendingPreview = false
            Qt.callLater(function() {
                if (!renderer.running && !liveRefresh.running && livePreview.checked) window.launch(true)
            })
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 800
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Label { text: "Mescaline"; font.pixelSize: 24; font.bold: true; Layout.fillWidth: true }
                Button { text: "Settings"; onClicked: settingsDialog.open(); Accessible.name: "Open settings" }
                Button { text: "Expressions"; onClicked: expressionHelp.open(); Accessible.name: "Open scalar expression cheat sheet" }
            }

            ScrollView {
                id: scroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                contentHeight: form.implicitHeight
                clip: true

                ColumnLayout {
                    id: form
                    width: scroll.availableWidth
                    spacing: 14

                    Label { text: "Render mode"; font.bold: true }
                    ComboBox {
                        id: mode
                        Layout.fillWidth: true
                        model: ["Algorithm", "Scalar expression", "RGB expressions"]
                        Accessible.name: "Render mode"
                    }

                    ColumnLayout {
                        visible: mode.currentIndex === 0
                        Layout.fillWidth: true
                        Label { text: "Algorithm" }
                        ComboBox {
                            id: algorithm
                            Layout.fillWidth: true
                            model: ["checkerboard", "lasagna", "carreaux"]
                            Accessible.name: "Algorithm"
                        }
                        Label { text: "Scale" }
                        TextField {
                            id: scale
                            objectName: "scaleInput"
                            Layout.fillWidth: true
                            text: "1"
                            onTextChanged: window.normalizeFraction(scale)
                            Accessible.name: "Scale"
                        }
                    }

                    ColumnLayout {
                        visible: mode.currentIndex === 1
                        Layout.fillWidth: true
                        Label { text: "Scalar expression" }
                        TextArea {
                            id: scalar
                            objectName: "scalarExpression"
                            Layout.fillWidth: true
                            text: "sin(x * pi * 8) * 0.5 + 0.5"
                            onTextChanged: window.normalizeExpression(scalar)
                            wrapMode: TextEdit.Wrap
                            Accessible.name: "Scalar expression"
                        }
                        Label { text: "Palette" }
                        ComboBox {
                            id: palette
                            Layout.fillWidth: true
                            model: ["grayscale", "monochrome", "viridis", "plasma", "magma", "inferno", "turbo"]
                            Accessible.name: "Palette"
                        }
                    }

                    ColumnLayout {
                        visible: mode.currentIndex === 2
                        Layout.fillWidth: true
                        Label { text: "Red expression" }
                        TextField { id: red; objectName: "redExpression"; Layout.fillWidth: true; text: "x"; onTextChanged: window.normalizeExpression(red); Accessible.name: "Red expression" }
                        Label { text: "Green expression" }
                        TextField { id: green; Layout.fillWidth: true; text: "y"; onTextChanged: window.normalizeExpression(green); Accessible.name: "Green expression" }
                        Label { text: "Blue expression" }
                        TextField { id: blue; Layout.fillWidth: true; text: "t"; onTextChanged: window.normalizeExpression(blue); Accessible.name: "Blue expression" }
                    }

                    ColumnLayout {
                        visible: mode.currentIndex === 0 || (mode.currentIndex === 1 && palette.currentText === "monochrome")
                        Layout.fillWidth: true
                        Label { text: "Color (RRGGBB)" }
                        TextField {
                            id: color
                            Layout.fillWidth: true
                            text: "ffffff"
                            maximumLength: 6
                            validator: RegularExpressionValidator { regularExpression: /[0-9a-fA-F]{6}/ }
                            Accessible.name: "Color, six hexadecimal digits"
                        }
                    }

                    ColumnLayout {
                        visible: mode.currentIndex !== 0
                        Layout.fillWidth: true
                        Label { text: "Seed (unsigned integer)" }
                        TextField {
                            id: seed
                            Layout.fillWidth: true
                            text: "0"
                            validator: RegularExpressionValidator { regularExpression: /[0-9]+/ }
                            Accessible.name: "Random seed"
                        }
                    }

                    Label { text: "Canvas"; font.bold: true }
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: "Width" }
                            SpinBox { id: canvasWidth; objectName: "canvasWidth"; from: 1; to: 100000; value: 1000; editable: true; Layout.fillWidth: true; Accessible.name: "Canvas width" }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: "Height" }
                            SpinBox { id: canvasHeight; from: 1; to: 100000; value: 1000; editable: true; Layout.fillWidth: true; Accessible.name: "Canvas height" }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: "Range mode" }
                        ComboBox { id: rangeMode; model: ["wrap", "clamp"]; Layout.fillWidth: true; Accessible.name: "Range mode" }
                    }

                    Label { text: "Output"; font.bold: true }
                    Label { text: "File ending in .ppm or .gif, or a frame directory"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    TextField {
                        id: output
                        Layout.fillWidth: true
                        text: "outputs/image.ppm"
                        Accessible.name: "Output path"
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: "Frames" }
                            SpinBox { id: frames; from: 1; to: 1000; value: 1; editable: true; Layout.fillWidth: true; Accessible.name: "Frame count" }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: "GIF FPS" }
                            SpinBox { id: fps; from: 1; to: 1000; value: 30; editable: true; Layout.fillWidth: true; Accessible.name: "GIF frames per second" }
                        }
                    }
                    CheckBox { id: force; text: "Replace existing output (--force)"; Accessible.name: text }

                }
            }

            ProgressBar { Layout.fillWidth: true; from: 0; to: 100; value: renderer.progress }
            Label { text: renderer.status; Layout.fillWidth: true; wrapMode: Text.WordWrap; Accessible.name: "Render status" }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: renderer.details.length > 0 ? 96 : 0
                visible: renderer.details.length > 0
                TextArea { readOnly: true; text: renderer.details; wrapMode: TextEdit.Wrap; Accessible.name: "Render errors and warnings" }
            }
            RowLayout {
                Layout.fillWidth: true
                Button { text: "Preview"; enabled: !renderer.running; onClicked: window.launch(true); Accessible.name: text }
                Button { text: "Render"; enabled: !renderer.running; onClicked: window.launch(false); Accessible.name: text }
                Button { text: "Cancel"; enabled: renderer.running; onClicked: renderer.cancel(); Accessible.name: text }
                Item { Layout.fillWidth: true }
                Button { text: "Open result"; enabled: !renderer.running && renderer.resultPath.length > 0; onClicked: renderer.openResult(); Accessible.name: text }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 800
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Label { text: "Export preview"; font.pixelSize: 24; font.bold: true; Layout.fillWidth: true }
                CheckBox {
                    id: livePreview
                    objectName: "livePreview"
                    text: "Live"
                    Accessible.name: "Live preview"
                    onToggled: {
                        if (checked) liveRefresh.restart()
                        else {
                            liveRefresh.stop()
                            window.pendingPreview = false
                            if (renderer.previewRunning) renderer.cancel()
                        }
                    }
                }
            }
            Label {
                objectName: "previewDescription"
                text: "First frame: " + (reducedPreview.checked ? previewScale.text + "%" : "full") +
                      " resolution · " + (livePreview.checked ? "updates after edits" : "click Preview to update")
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Rectangle {
                objectName: "previewPane"
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Qt.rgba(0.5, 0.5, 0.5, 0.15)
                Image {
                    anchors.fill: parent
                    anchors.margins: 8
                    source: renderer.previewUrl
                    sourceSize: Qt.size(1024, 0)
                    fillMode: Image.PreserveAspectFit
                    cache: false
                    asynchronous: false
                    Accessible.name: "Export preview"
                }
                Label {
                    anchors.centerIn: parent
                    text: "Click Preview to see what will be exported"
                    visible: renderer.previewUrl.length === 0
                }
            }
        }
    }
}
