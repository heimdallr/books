import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.3

Menu
{
	title: qsTranslate("Tray", "Help")

	MessageDialog
	{
	    id: aboutDialogID
	    title: qsTranslate("Tray", "About Flibrary")
	    text: qsTranslate("Tray", "Another e-library book cataloger")
		informativeText: "<a href=\"%1\">%1</a>".arg("https://github.com/heimdallr/books")
		icon: StandardIcon.Information
		standardButtons: StandardButton.Ok
	}

	MenuItem
	{
		text: qsTranslate("Tray", "About Flibrary...")
		onTriggered: aboutDialogID.open()
	}
}
