import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.1

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

	ExclusiveGroup
	{
		id: exclusiveGroupID
	}

    Menu
	{
		id: collectionsMenuID

		readonly property string currentCollectionId: collectionController.currentCollectionId

        title: qsTranslate("Tray", "Select collection")

		enabled: false

		Instantiator
		{
			model: collectionController.GetModel()
			MenuItem
			{
				text: Title
				checkable: true
				checked: Value === collectionsMenuID.currentCollectionId
				exclusiveGroup: exclusiveGroupID
				onTriggered: collectionController.currentCollectionId = Value
			}

			onObjectAdded:
			{
				collectionsMenuID.insertItem(index, object)
				collectionsMenuID.enabled = true
			}
			onObjectRemoved:
			{
				collectionsMenuID.removeItem(object)
				if (collectionsMenuID.items.count == 0)
					collectionsMenuID.enabled = false
			}
		}
    }

    MenuItem
	{
		enabled: collectionsMenuID.enabled
        text: qsTranslate("Tray", "Remove collection")
        onTriggered: removeConfirmDialogID.open()
    }
}
