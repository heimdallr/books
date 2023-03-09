import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.1

import "qrc:/Core"
import "qrc:/Dialogs"

Menu
{
	readonly property var collectionController: guiController.GetCollectionController()

	title: qsTranslate("Tray", "Collections")

    MenuItem
	{
		text: qsTranslate("Tray", "Add new collection...")
		onTriggered: collectionController.GetAddModeDialogController().visible = true
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
		onTriggered: collectionController.GetRemoveCollectionConfirmDialogController().visible = true
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
