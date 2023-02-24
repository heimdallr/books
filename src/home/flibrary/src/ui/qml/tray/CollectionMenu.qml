import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.1

import "qrc:/Core"
import "qrc:/Dialogs"

Menu
{
	readonly property var collectionController: guiController.GetCollectionController()

	title: qsTranslate("Tray", "Collections")

	MessageDialog
	{
		id: removeDatabaseConfirmDialogID
		title: qsTranslate("Common", "Warning")
		text: qsTranslate("Collection", "Delete collection database as well?")
		standardButtons: StandardButton.Yes | StandardButton.No | StandardButton.Cancel
		icon: StandardIcon.Question
		onYes: collectionController.RemoveCurrentCollection(true)
		onNo: collectionController.RemoveCurrentCollection(false)
	}

	MessageDialog
	{
		id: removeConfirmDialogID
		title: qsTranslate("Common", "Warning")
		text: qsTranslate("Collection", "Are you sure you want to delete the collection?")
		standardButtons: StandardButton.Yes | StandardButton.No
		icon: StandardIcon.Warning
		onYes: removeDatabaseConfirmDialogID.open()
	}

	MessageDialog
	{
		id: updateConfirmDialogID
		title: qsTranslate("Common", "Warning")
		text: qsTranslate("Collection", "Looks like the collection has been updated. Apply changes?")
		standardButtons: StandardButton.Yes | StandardButton.Cancel | StandardButton.Discard
		icon: StandardIcon.Question
		visible: collectionController.hasUpdate
		onYes: collectionController.ApplyUpdate()
		onRejected: collectionController.hasUpdate = false
		onDiscard: collectionController.DiscardUpdate()
	}

    MenuItem
	{
		text: qsTranslate("Tray", "Add new collection...")
		onTriggered: collectionController.addMode = true
	}

	DynamicMenu
	{
		id: collectionsMenuID

		readonly property string currentId: collectionController.currentCollectionId

		title: qsTranslate("Tray", "Select collection")

		model: collectionController.GetModel()
		delegate: MenuItem
		{
			text: Title
			checked: Value === collectionsMenuID.currentId
			onTriggered: if (Value !== collectionsMenuID.currentId)
				collectionController.currentCollectionId = Value
		}
	}

	MenuItem
	{
		enabled: collectionsMenuID.enabled
		text: qsTranslate("Tray", "Remove collection")
		onTriggered: removeConfirmDialogID.open()
    }

	Menu
	{
		title: qsTranslate("Tray", "User data")

	    MenuItem
		{
			text: qsTranslate("Tray", "Export")
			onTriggered: guiController.BackupUserData()
		}

	    MenuItem
		{
			text: qsTranslate("Tray", "Import")
			onTriggered: guiController.RestoreUserData()
		}
	}
}
