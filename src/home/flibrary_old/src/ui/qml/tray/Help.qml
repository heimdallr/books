import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.3

Menu
{
	title: qsTranslate("Tray", "Help")

	MenuItem
	{
		text: qsTranslate("Tray", "About Flibrary...")
		onTriggered: guiController.GetAboutDialogController().visible = true
	}
}
