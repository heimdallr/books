import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.2

import "qrc:/Core"
import "qrc:/Dialogs"

Menu
{
	id: menuID

	property var controller
	property int bookId: 0
	property bool isDictionary: false

	MenuItem
	{
		visible: !menuID.isDictionary
		text: qsTranslate("BookContextMenu", "Read")
        onTriggered: controller.Read(menuID.bookId)
	}

	Menu
	{
		title: qsTranslate("BookContextMenu", "Selection")

		MenuItem
		{
			id: selectAllID
			text: qsTranslate("BookContextMenu", "Select all")
	        onTriggered: controller.SelectAll()
		}

		MenuItem
		{
			id: deselectAllID
			text: qsTranslate("BookContextMenu", "Deselect all")
	        onTriggered: controller.DeselectAll()
		}

		MenuItem
		{
			id: invertSelectionID
			text: qsTranslate("BookContextMenu", "Invert selection")
	        onTriggered: controller.InvertSelection()
		}
	}

	Menu
	{
		id: sendToDeviceMenuID
		title: qsTranslate("BookContextMenu", "Send to device")
		visible: !menuID.isDictionary

		function save(archivate)
		{
			const folder = fileDialog.SelectFolder(qsTranslate("FileDialog", "Select destination folder"), uiSettings.pathRecentExport)
			if (folder === "")
				return

			uiSettings.pathRecentExport = folder
			archivate
				? controller.WriteToArchive(folder, menuID.bookId)
				: controller.WriteToFile(folder, menuID.bookId)
		}

		MenuItem
		{
			text: qsTranslate("BookContextMenu", "In zip archive")
			onTriggered: sendToDeviceMenuID.save(true)
		}

		MenuItem
		{
			text: qsTranslate("BookContextMenu", "In original format")
			onTriggered: sendToDeviceMenuID.save(false)
		}
	}

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

	MenuItem
	{
		id: removeID
		text: qsTranslate("BookContextMenu", "Remove book")
        onTriggered: controller.Remove(menuID.bookId)
	}

	MenuItem
	{
		id: restoreID
		text: qsTranslate("BookContextMenu", "Undo book deletion")
        onTriggered: controller.Restore(menuID.bookId)
	}

	onAboutToShow:
	{
		selectAllID.enabled = !controller.AllSelected()
		deselectAllID.enabled = controller.HasSelected()
		removeID.visible = controller.RemoveAvailable(menuID.bookId)
		restoreID.visible = controller.RestoreAvailable(menuID.bookId)
	}
}
