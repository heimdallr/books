import QtQuick 2.12
import QtQuick.Dialogs 1.1

Item
{
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
