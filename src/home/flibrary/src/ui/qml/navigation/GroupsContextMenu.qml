import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.1

import "qrc:/Dialogs"

Menu
{
	id: groupsContextMenuID

	readonly property var controller: guiController.GetGroupsModelController()
	property int navigationId: 0

	InputNewGroupName
	{
		id: inputGroupNameDialogID
		controller: groupsContextMenuID.controller
		onAccepted: groupsContextMenuID.controller.CreateNewGroup(text)
	}

	MessageDialog
	{
	    id: removeConfirmDialogID
	    title: qsTranslate("Common", "Warning")
	    text: qsTranslate("GroupsMenu", "Are you sure you want to delete the group?")
		standardButtons: StandardButton.Yes | StandardButton.No
		icon: StandardIcon.Warning
	    onYes: controller.RemoveGroup(navigationId)
	}

	MenuItem
	{
		text: qsTranslate("GroupsMenu", "Create new...")
		onTriggered: inputGroupNameDialogID.visible = true
	}

	MenuItem
	{
		text: qsTranslate("GroupsMenu", "Remove")
		onTriggered: removeConfirmDialogID.open()
	}
}
