import QtQuick 2.12
import QtQuick.Dialogs 1.1

Item
{
	Item
	{
		id: guiControllerDialogsID

		MessageDialogCustom
		{
			controller: guiController.GetAboutDialogController()
		    title: qsTranslate("Tray", "About Flibrary")
		    text: qsTranslate("Tray", "Another e-library book cataloger\nVersion: %1").arg(guiController.GetVersion())
			informativeText: "<a href=\"%1\">%1</a>".arg("https://github.com/heimdallr/books")
			icon: StandardIcon.Information
			standardButtons: StandardButton.Ok
		}

		MessageDialogCustom
		{
			controller: guiController.GetRestartDialogController()
		    title: qsTranslate("Common", "Warning")
		    text: qsTranslate("Collection", "You must restart the application to apply the changes.\nRestart now?")
			standardButtons: StandardButton.Yes | StandardButton.No
			icon: StandardIcon.Question
		}
	}

	Item
	{
		id: groupDialogsID

		readonly property var controller: guiController.GetGroupsModelController()

		MessageDialogCustom
		{
			controller: groupDialogsID.controller.GetRemoveGroupConfirmDialogController()
		    title: qsTranslate("Common", "Warning")
		    text: qsTranslate("GroupsMenu", "Are you sure you want to delete the group?")
			standardButtons: StandardButton.Yes | StandardButton.No
			icon: StandardIcon.Warning
		}
	}

	Item
	{
		id: collectionDialogsID

		readonly property var controller: guiController.GetCollectionController()

		MessageDialogCustom
		{
			controller: collectionDialogsID.controller.GetRemoveDatabaseConfirmDialogController()
			title: qsTranslate("Common", "Warning")
			text: qsTranslate("Collection", "Delete collection database as well?")
			standardButtons: StandardButton.Yes | StandardButton.No | StandardButton.Cancel
			icon: StandardIcon.Question
		}

		MessageDialogCustom
		{
			controller: collectionDialogsID.controller.GetRemoveCollectionConfirmDialogController()
			title: qsTranslate("Common", "Warning")
			text: qsTranslate("Collection", "Are you sure you want to delete the collection?")
			standardButtons: StandardButton.Yes | StandardButton.No
			icon: StandardIcon.Warning
		}

		MessageDialogCustom
		{
			controller: collectionDialogsID.controller.GetHasUpdateDialogController()
			title: qsTranslate("Common", "Warning")
			text: qsTranslate("Collection", "Looks like the collection has been updated. Apply changes?")
			standardButtons: StandardButton.Yes | StandardButton.Cancel | StandardButton.Discard
			icon: StandardIcon.Question
		}
	}
}
