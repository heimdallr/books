import QtQuick 2.15
import QtQuick.Controls 1.4

import "qrc:/Core"
import "qrc:/Dialogs"

Menu
{
	id: menuID
	title: qsTranslate("GroupsMenu", "Groups")

	readonly property var controller: guiController.GetGroupsModelController()
	property int bookId: 0

	InputNewGroupName
	{
		id: inputGroupNameDialogID
		controller: menuID.controller
		onAccepted: controller.AddToNew(text)
	}

	DynamicMenu
	{
		id: groupAddID
		title: qsTranslate("GroupsMenu", "Add to")
		checkable: false
		alwaysEnabled: true

		model: menuID.controller.GetAddToModel()
		delegate: MenuItem
		{
			text: Title
			onTriggered: menuID.controller.AddTo(Value)
		}

		MenuSeparator
		{
			visible: menuID.controller.toAddExists
		}

		MenuItem
		{
			text: qsTranslate("GroupsMenu", "New group...")
			onTriggered:
			{
				inputGroupNameDialogID.visible = true
				menuID.controller.CheckNewName(inputStringDialogID.text)
			}
		}
	}

	DynamicMenu
	{
		id: groupRemoveID
		title: qsTranslate("GroupsMenu", "Remove from")
		checkable: false

		model: menuID.controller.GetRemoveFromModel()
		delegate: MenuItem
		{
			text: Title
			onTriggered: menuID.controller.RemoveFrom(Value)
		}

		MenuSeparator {}

		MenuItem
		{
			text: qsTranslate("GroupsMenu", "All")
			onTriggered: menuID.controller.RemoveFromAll()
		}
	}

	onAboutToShow: controller.Reset(menuID.bookId)
}
