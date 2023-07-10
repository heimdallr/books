import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.1

import "qrc:/Dialogs"

Menu
{
	id: groupsContextMenuID

	readonly property var modelController: guiController.GetGroupsModelController()

	InputNewGroupName
	{
		id: inputGroupNameDialogID
		controller: modelController
		onAccepted: controller.CreateNewGroup(text)
	}

	MenuItem
	{
		text: qsTranslate("GroupsMenu", "Create new...")
		onTriggered: inputGroupNameDialogID.visible = true
	}

	MenuItem
	{
		text: qsTranslate("GroupsMenu", "Remove")
		onTriggered: modelController.GetRemoveGroupConfirmDialogController().visible = true
	}
}
