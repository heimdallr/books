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

	AddCollection
	{
		id: addCollectionID
	}

    MenuItem
	{
        text: qsTranslate("Tray", "Add new collection...")
        onTriggered: addCollectionID.show()
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
