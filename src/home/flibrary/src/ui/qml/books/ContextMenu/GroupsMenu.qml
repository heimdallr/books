import QtQuick 2.15
import QtQuick.Controls 1.4

import "qrc:/Core"
import "qrc:/Dialogs"

Menu
{
	id: groupID
	title: qsTranslate("BookContextMenu", "Groups")

	readonly property var controller: guiController.GetGroupsModelController()

	InputStringDialog
	{
		id: inputStringDialogID
		title: qsTranslate("BookContextMenu", "Input new group name")
		inputStringTitle: qsTranslate("BookContextMenu", "Group name")
		text: qsTranslate("BookContextMenu", "New group")
		errorText: groupID.controller.errorText
		okEnabled: text !== "" && errorText === "" && !groupID.controller.checkNewNameInProgress
		onAccepted: groupID.controller.AddToNew(text)
		onTextChanged: groupID.controller.CheckNewName(text)
	}

	DynamicMenu
	{
		id: groupAddID
		title: qsTranslate("BookContextMenu", "Add to")
		checkable: false

		model: groupID.controller.GetAddToModel()
		delegate: MenuItem
		{
			text: Title
			onTriggered:
			{
				if (parseInt(Value) < 0)
				{
					inputStringDialogID.visible = true
					groupID.controller.CheckNewName(inputStringDialogID.text)
				}
				else
				{
					groupID.controller.AddTo(Value)
				}
			}
		}
	}

	DynamicMenu
	{
		id: groupRemoveID
		title: qsTranslate("BookContextMenu", "Remove from")
		checkable: false

		model: groupID.controller.GetRemoveFromModel()
		delegate: MenuItem
		{
			text: Title
			onTriggered: groupID.controller.RemoveFrom(Value)
		}
	}

	onAboutToShow: controller.Reset(menuID.bookId)
}
