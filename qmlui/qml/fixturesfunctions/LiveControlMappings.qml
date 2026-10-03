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

    CustomPopupDialog
    {
        id: messagePopup
        title: qsTr("Live MIDI mapping")
        standardButtons: Dialog.Ok
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

                Rectangle
                {
                    width: parent.width
                    height: UISettings.listItemHeight
                    color: UISettings.bgStronger
                    RobotoText
                    {
                        anchors.fill: parent
                        anchors.leftMargin: 5
                        label: qsTr("Selected fixture presets")
                        fontBold: true
                    }
                }

                Repeater
                {
                    model: liveControlManager.presetTargets
                    delegate: mappingRow
                    property bool presetRows: true
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
        id: mappingRow
        Column
        {
            id: rowRoot
            width: body.width
            property bool isPreset: modelData.group !== undefined && modelData.preset !== undefined
            property string targetId: isPreset ? "preset" : modelData.id
            property var presetData: isPreset ? modelData : ({})
            property var assigned: root.mappingsFor(targetId, presetData)

            Rectangle
            {
                visible: !rowRoot.isPreset && (index === 0
                         || liveControlManager.targets[index - 1].group !== modelData.group)
                width: parent.width
                height: visible ? UISettings.listItemHeight : 0
                color: UISettings.bgStronger
                RobotoText
                {
                    anchors.fill: parent
                    anchors.leftMargin: 5
                    label: rowRoot.isPreset ? "" : modelData.group
                    fontBold: true
                }
            }

            Rectangle
            {
                width: parent.width
                height: UISettings.iconSizeMedium
                color: UISettings.bgMedium
                RowLayout
                {
                    anchors.fill: parent
                    anchors.leftMargin: 5
                    spacing: 3
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
                model: rowRoot.assigned
                delegate: Rectangle
                {
                    width: rowRoot.width
                    height: UISettings.listItemHeight
                    color: "transparent"
                    RowLayout
                    {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        RobotoText
                        {
                            Layout.fillWidth: true
                            height: parent.height
                            label: modelData.sourceName
                            labelColor: modelData.valid ? UISettings.fgMain : "orange"
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
        }
    }
}
