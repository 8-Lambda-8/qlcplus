/*
  Q Light Controller Plus
  LiveControlMappings.qml

  Licensed under the Apache License, Version 2.0.
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

// QML files are flattened into qrc:/ by qmlui.qrc, like the other
// Fixtures & Functions panels, so shared controls live in the same URL scope.
import "."

Rectangle
{
    id: root
    color: UISettings.bgStrong
    property var expandedSections: ({
        "intensity": true,
        "color": true,
        "position": true,
        "beam": true,
        "actions": true
    })

    function sectionExpanded(sectionId)
    {
        return expandedSections[sectionId] !== false
    }

    function toggleSection(sectionId)
    {
        var updated = ({})
        for (var key in expandedSections)
            updated[key] = expandedSections[key]
        updated[sectionId] = !sectionExpanded(sectionId)
        expandedSections = updated
    }

    function mappingsFor(target, preset)
    {
        var result = []
        var all = liveControlManager.mappings
        for (var i = 0; i < all.length; ++i)
        {
            if (all[i].target !== target)
                continue
            if (target === "preset" && (all[i].preset.group !== preset.group
                    || all[i].preset.preset !== preset.preset
                    || all[i].preset.name !== preset.name))
                continue
            result.push(all[i])
        }
        return result
    }

    function openManual(target, preset)
    {
        if (liveControlManager.inputUniverses.length === 0)
        {
            messagePopup.message = qsTr("Patch a MIDI input to an input universe before creating a live mapping.")
            messagePopup.open()
            return
        }
        manualPopup.target = target
        manualPopup.preset = preset || ({})
        manualPopup.open()
    }

    function startLearn(target, preset)
    {
        if (liveControlManager.inputUniverses.length === 0)
        {
            messagePopup.message = qsTr("Patch a MIDI input to an input universe before starting MIDI learn.")
            messagePopup.open()
            return
        }
        liveControlManager.beginLearn(target, preset || ({}))
    }

    function presetGroups(presets, section)
    {
        var definitions = [
            { id: "color", section: "Color", name: qsTr("Colors"), icon: "qrc:/colorwheel.svg", items: [] },
            { id: "macro", section: "Color", name: qsTr("Macros"), icon: "qrc:/colorwheel.svg", items: [] },
            { id: "gobo", section: "Beam", name: qsTr("Gobos"), icon: "qrc:/gobo.svg", items: [] },
            { id: "shutter", section: "Beam", name: qsTr("Shutter"), icon: "qrc:/shutter.svg", items: [] },
            { id: "other", section: "Beam", name: qsTr("Other capabilities"), icon: "qrc:/beam.svg", items: [] }
        ]
        for (var i = 0; i < presets.length; ++i)
        {
            var category = presets[i].category || "other"
            for (var j = 0; j < definitions.length; ++j)
            {
                if (definitions[j].id === category)
                {
                    definitions[j].items.push(presets[i])
                    break
                }
            }
        }

        var result = []
        for (var k = 0; k < definitions.length; ++k)
        {
            if (definitions[k].section === section && definitions[k].items.length > 0)
                result.push(definitions[k])
        }
        return result
    }

    CustomPopupDialog
    {
        id: messagePopup
        title: qsTr("Live MIDI mapping")
        standardButtons: Dialog.Ok
    }

    PopupCustomFeedback
    {
        id: feedbackPopup
        widgetObjRef: liveControlManager
    }

    CustomPopupDialog
    {
        id: manualPopup
        title: qsTr("Manual live-control input")
        standardButtons: Dialog.Cancel | Dialog.Ok
        property string target: ""
        property var preset: ({})

        onOpened:
        {
            universeCombo.model = liveControlManager.inputUniverses
            universeCombo.currentIndex = 0
        }
        onAccepted:
        {
            if (!liveControlManager.addMapping(target, universeCombo.currValue,
                                               channelSpin.value - 1, preset))
            {
                messagePopup.message = qsTr("The mapping could not be added. Check that the input is patched and that the same mapping does not already exist.")
                messagePopup.open()
            }
        }

        contentItem: GridLayout
        {
            columns: 2
            RobotoText { label: qsTr("Patched input"); height: UISettings.listItemHeight }
            CustomComboBox { id: universeCombo; Layout.fillWidth: true; height: UISettings.listItemHeight }
            RobotoText { label: qsTr("Channel"); height: UISettings.listItemHeight }
            CustomSpinBox { id: channelSpin; from: 1; to: 65536; value: 1; Layout.fillWidth: true }
        }
    }

    ColumnLayout
    {
        anchors.fill: parent
        spacing: 2

        Rectangle
        {
            Layout.fillWidth: true
            height: UISettings.iconSizeDefault
            color: UISettings.bgStronger
            RowLayout
            {
                anchors.fill: parent
                anchors.margins: 3
                RobotoText
                {
                    label: liveControlManager.learning
                           ? qsTr("Move a MIDI control for %1").arg(liveControlManager.learningTarget)
                           : (liveControlManager.inputUniverses.length === 0
                              ? qsTr("Patch a MIDI input first") : qsTr("Live MIDI mappings"))
                    fontBold: true
                    Layout.fillWidth: true
                    labelColor: liveControlManager.inputUniverses.length === 0 ? "orange" : UISettings.fgMain
                }
                IconButton
                {
                    visible: liveControlManager.learning
                    width: UISettings.iconSizeMedium
                    height: width
                    faSource: FontAwesome.fa_xmark
                    tooltip: qsTr("Cancel MIDI learn")
                    onClicked: liveControlManager.cancelLearn()
                }
                IconButton
                {
                    width: UISettings.iconSizeMedium
                    height: width
                    faSource: FontAwesome.fa_trash
                    faColor: "crimson"
                    tooltip: qsTr("Remove all live MIDI mappings")
                    onClicked: liveControlManager.clearMappings()
                }
            }
        }

        Flickable
        {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: body.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: CustomScrollBar { id: mappingScrollBar }

            Column
            {
                id: body
                // Attached scrollbars overlay Flickable content. Keep the row
                // action buttons and mapping removal buttons clear of it.
                width: parent.width - (mappingScrollBar.visible
                                       ? UISettings.scrollBarWidth + 2 : 0)
                spacing: 2

                Repeater
                {
                    model: liveControlManager.targets
                    delegate: mappingRow
                }

                RobotoText
                {
                    visible: liveControlManager.presetTargets.length === 0
                    width: parent.width
                    height: UISettings.listItemHeight * 2
                    label: qsTr("Select fixtures with shutter, color-wheel or gobo capabilities to map presets.")
                    wrapText: true
                }
            }
        }
    }

    Component
    {
        id: presetGroup
        Column
        {
            id: presetGroupRoot
            width: body.width
            property bool expanded: false

            Rectangle
            {
                width: parent.width
                height: UISettings.iconSizeMedium
                color: groupMouse.containsMouse ? UISettings.hover : UISettings.bgMedium

                MouseArea
                {
                    id: groupMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: presetGroupRoot.expanded = !presetGroupRoot.expanded
                }

                RowLayout
                {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 5
                    spacing: 4

                    Text
                    {
                        Layout.alignment: Qt.AlignVCenter
                        text: presetGroupRoot.expanded ? FontAwesome.fa_chevron_down
                                                       : FontAwesome.fa_chevron_right
                        color: UISettings.fgMedium
                        font.family: UISettings.fontAwesomeFontName
                        font.pixelSize: UISettings.textSizeDefault
                    }
                    Image
                    {
                        Layout.preferredWidth: parent.height - 8
                        Layout.preferredHeight: width
                        source: modelData.icon
                        sourceSize: Qt.size(width, height)
                    }
                    RobotoText
                    {
                        Layout.fillWidth: true
                        height: parent.height
                        label: modelData.name
                        fontBold: true
                    }
                    RobotoText
                    {
                        Layout.preferredWidth: implicitWidth
                        height: parent.height
                        label: modelData.items.length.toString()
                        labelColor: UISettings.fgMedium
                    }
                }
            }

            Repeater
            {
                model: presetGroupRoot.expanded ? modelData.items : []
                delegate: mappingRow
            }
        }
    }

    Component
    {
        id: mappingRow
        Column
        {
            id: rowRoot
            width: body.width
            property bool isPreset: modelData.group !== undefined && modelData.preset !== undefined
            property string targetId: isPreset ? "preset" : modelData.id
            property string sectionId: isPreset ? "" : modelData.groupId
            property var presetData: isPreset ? modelData : ({})
            property var assigned: root.mappingsFor(targetId, presetData)

            Rectangle
            {
                visible: !rowRoot.isPreset && (index === 0
                         || liveControlManager.targets[index - 1].group !== modelData.group)
                width: parent.width
                height: visible ? UISettings.iconSizeDefault : 0
                color: UISettings.bgControl

                MouseArea
                {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.toggleSection(rowRoot.sectionId)
                }

                RowLayout
                {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6

                    Text
                    {
                        Layout.alignment: Qt.AlignVCenter
                        text: root.sectionExpanded(rowRoot.sectionId)
                              ? FontAwesome.fa_chevron_down : FontAwesome.fa_chevron_right
                        color: UISettings.fgMain
                        font.family: UISettings.fontAwesomeFontName
                        font.pixelSize: UISettings.textSizeDefault
                    }
                    Item
                    {
                        Layout.preferredWidth: parent.height - 12
                        Layout.preferredHeight: width
                        property string iconSource: rowRoot.targetId === "intensity" ? "qrc:/intensity.svg"
                                : rowRoot.targetId === "hue" ? "qrc:/color.svg"
                                : rowRoot.targetId === "pan" ? "qrc:/position.svg"
                                : rowRoot.targetId === "zoom" ? "qrc:/beam.svg" : ""

                        Image
                        {
                            visible: parent.iconSource !== ""
                            anchors.fill: parent
                            source: parent.iconSource
                            sourceSize: Qt.size(width, height)
                        }
                        Text
                        {
                            visible: rowRoot.targetId === "highlight"
                            anchors.centerIn: parent
                            text: FontAwesome.fa_bolt
                            color: UISettings.fgMain
                            font.family: UISettings.fontAwesomeFontName
                            font.pixelSize: parent.height * 0.7
                        }
                    }
                    RobotoText
                    {
                        Layout.fillWidth: true
                        height: parent.height
                        label: rowRoot.isPreset ? "" : modelData.group
                        fontBold: true
                        fontSize: UISettings.textSizeDefault + 1
                    }
                }
            }

            Rectangle
            {
                visible: rowRoot.isPreset || root.sectionExpanded(rowRoot.sectionId)
                width: parent.width
                height: visible ? UISettings.iconSizeMedium : 0
                color: UISettings.bgMedium
                RowLayout
                {
                    anchors.fill: parent
                    anchors.leftMargin: 5
                    anchors.rightMargin: 2
                    spacing: 3
                    Item
                    {
                        width: parent.height
                        height: width
                        opacity: rowRoot.isPreset || contextManager.liveControlSupported(rowRoot.targetId)
                                 ? 1.0 : 0.4
                        property string iconSource: rowRoot.isPreset
                                ? (modelData.group === QLCChannel.Shutter ? "qrc:/shutter.svg"
                                   : modelData.group === QLCChannel.Gobo ? "qrc:/gobo.svg"
                                   : "qrc:/colorwheel.svg")
                                : modelData.icon

                        Image
                        {
                            visible: parent.iconSource !== ""
                            anchors.centerIn: parent
                            width: parent.width - 6
                            height: width
                            source: parent.iconSource
                            sourceSize: Qt.size(width, height)
                        }
                        Text
                        {
                            visible: rowRoot.targetId === "highlight"
                            anchors.centerIn: parent
                            text: FontAwesome.fa_bolt
                            color: UISettings.fgMain
                            font.family: UISettings.fontAwesomeFontName
                            font.pixelSize: parent.height * 0.65
                        }
                        Rectangle
                        {
                            visible: rowRoot.targetId.endsWith("-next")
                                     || rowRoot.targetId.endsWith("-previous")
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            width: parent.width * 0.48
                            height: width
                            radius: width / 2
                            color: UISettings.bgStronger
                            Text
                            {
                                anchors.centerIn: parent
                                text: rowRoot.targetId.endsWith("-next")
                                      ? FontAwesome.fa_arrow_right : FontAwesome.fa_arrow_left
                                color: UISettings.fgMain
                                font.family: UISettings.fontAwesomeFontName
                                font.pixelSize: parent.height * 0.65
                            }
                        }
                    }
                    RobotoText
                    {
                        Layout.fillWidth: true
                        height: parent.height
                        label: rowRoot.isPreset ? modelData.name : modelData.name
                        labelColor: rowRoot.isPreset || contextManager.liveControlSupported(rowRoot.targetId)
                                    ? UISettings.fgMain : UISettings.fgMedium
                    }
                    IconButton
                    {
                        width: parent.height
                        height: width
                        faSource: FontAwesome.fa_wand_magic_sparkles
                        faColor: liveControlManager.learningTarget === rowRoot.targetId ? "cyan" : UISettings.fgMain
                        tooltip: qsTr("Learn MIDI input")
                        onClicked: root.startLearn(rowRoot.targetId, rowRoot.presetData)
                    }
                    IconButton
                    {
                        width: parent.height
                        height: width
                        faSource: FontAwesome.fa_hand_pointer
                        tooltip: qsTr("Select input manually")
                        onClicked: root.openManual(rowRoot.targetId, rowRoot.presetData)
                    }
                }
            }

            Repeater
            {
                model: (rowRoot.isPreset || root.sectionExpanded(rowRoot.sectionId))
                       ? rowRoot.assigned : []
                delegate: Rectangle
                {
                    x: 12
                    width: rowRoot.width - x
                    height: UISettings.listItemHeight
                    color: UISettings.bgStronger

                    Rectangle
                    {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 3
                        color: modelData.valid ? UISettings.highlight : "orange"
                    }

                    RowLayout
                    {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 2
                        spacing: 4

                        RobotoText
                        {
                            Layout.fillWidth: true
                            height: parent.height
                            label: modelData.sourceName
                            labelColor: modelData.valid ? UISettings.fgMain : "orange"
                        }
                        IconButton
                        {
                            visible: modelData.customFeedback
                            width: UISettings.iconSizeMedium
                            height: width
                            imgSource: "qrc:/inputoutput.svg"
                            tooltip: qsTr("Custom feedback selection")
                            onClicked:
                            {
                                feedbackPopup.mappingId = modelData.id
                                feedbackPopup.open()
                            }
                        }
                        IconButton
                        {
                            width: UISettings.iconSizeMedium
                            height: width
                            faSource: FontAwesome.fa_minus
                            faColor: "crimson"
                            tooltip: qsTr("Remove mapping")
                            onClicked: liveControlManager.removeMapping(modelData.id)
                        }
                    }
                }
            }

            Repeater
            {
                model: !rowRoot.isPreset && root.sectionExpanded(rowRoot.sectionId)
                       && rowRoot.targetId === "macro-next"
                       ? root.presetGroups(liveControlManager.presetTargets, "Color")
                       : (!rowRoot.isPreset && root.sectionExpanded(rowRoot.sectionId)
                          && rowRoot.targetId === "gobo-wheel-next"
                          ? root.presetGroups(liveControlManager.presetTargets, "Beam") : [])
                delegate: presetGroup
            }
        }
    }
}
