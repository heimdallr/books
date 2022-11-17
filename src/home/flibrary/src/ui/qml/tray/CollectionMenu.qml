import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.1

import "qrc:/Core"
import "qrc:/Dialogs"

Menu
{
	title: qsTranslate("Tray", "Collections")

	MessageDialog
	{
	    id: removeConfirmDialogID
	    title: qsTranslate("Common", "Warning")
	    text: qsTranslate("Collection", "Are you sure you want to delete the collection?")
		standardButtons: StandardButton.Yes | StandardButton.No
	    onYes: collectionController.RemoveCurrentCollection()
	}

	MessageDialog
	{
	    id: updateConfirmDialogID
	    title: qsTranslate("Common", "Warning")
	    text: qsTranslate("Collection", "Looks like the collection has been updated. Apply changes?")
		standardButtons: StandardButton.Yes | StandardButton.Cancel | StandardButton.Discard
		visible: collectionController.hasUpdate
	    onYes: collectionController.ApplyUpdate()
		onRejected: collectionController.hasUpdate = false
		onDiscard: collectionController.DiscardUpdate()
	}

	AddCollection
	{
		id: addCollectionID
		visible: collectionController.addMode
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
}
