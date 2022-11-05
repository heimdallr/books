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
	    onYes:
		{
			console.log(`accepted`)
	    }
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

    Menu
	{
        title: qsTranslate("Tray", "Select collection")
        enabled: false
    }

    MenuItem
	{
        text: qsTranslate("Tray", "Remove collection")
        onTriggered: removeConfirmDialogID.open()
    }
}
