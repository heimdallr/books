import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.1

Item
{
	readonly property var collectionController: guiController.GetCollectionController()

	MessageDialogCustom
	{
		controller: collectionController.GetRemoveDatabaseConfirmDialogController()
		title: qsTranslate("Common", "Warning")
		text: qsTranslate("Collection", "Delete collection database as well?")
		standardButtons: StandardButton.Yes | StandardButton.No | StandardButton.Cancel
		icon: StandardIcon.Question
	}

	MessageDialogCustom
	{
		controller: collectionController.GetRemoveCollectionConfirmDialogController()
		title: qsTranslate("Common", "Warning")
		text: qsTranslate("Collection", "Are you sure you want to delete the collection?")
		standardButtons: StandardButton.Yes | StandardButton.No
		icon: StandardIcon.Warning
	}

	MessageDialogCustom
	{
		controller: collectionController.GetHasUpdateDialogController()
		title: qsTranslate("Common", "Warning")
		text: qsTranslate("Collection", "Looks like the collection has been updated. Apply changes?")
		standardButtons: StandardButton.Yes | StandardButton.Cancel | StandardButton.Discard
		icon: StandardIcon.Question
		visible: collectionController.GetHasUpdateDialogController().visible
		onYes: collectionController.ApplyUpdate()
		onRejected: collectionController.GetHasUpdateDialogController().visible = false
		onDiscard: collectionController.DiscardUpdate()
	}
}