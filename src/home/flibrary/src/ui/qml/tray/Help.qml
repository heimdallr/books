import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.1

Menu
{
	title: qsTranslate("Tray", "Help")

	MessageDialog
	{
	    id: aboutDialogID
	    title: qsTranslate("Tray", "About Flibrary")
	    text: qsTranslate("Tray", "Another e-library book classifier")
		standardButtons: StandardButton.Ok
	}

	MenuItem
	{
		text: qsTranslate("Tray", "About Flibrary...")
		onTriggered: aboutDialogID.open()
	}
}
