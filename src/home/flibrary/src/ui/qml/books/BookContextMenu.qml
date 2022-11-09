import QtQuick 2.15
import QtQuick.Controls 1.4

Menu
{
	id: menuID

	readonly property var controller: guiController.GetBooksModelController()
	property int bookId: 0

	Menu
	{
		title: qsTranslate("BookContextMenu", "Selection")

		MenuItem
		{
			id: selectAllID
			text: qsTranslate("BookContextMenu", "Select all")
	        onTriggered: controller.SelectAll()
		}

		MenuItem
		{
			id: deselectAllID
			text: qsTranslate("BookContextMenu", "Deselect all")
	        onTriggered: controller.DeselectAll()
		}

		MenuItem
		{
			id: invertSelectionID
			text: qsTranslate("BookContextMenu", "Invert selection")
	        onTriggered: controller.InvertSelection()
		}
	}

	MenuItem
	{
		id: sendID
		text: qsTranslate("BookContextMenu", "Send to device")
        onTriggered:
		{
			const folder = fileDialog.SelectFolder("")
			if (folder !== "")
				controller.Save(folder, menuID.bookId)
		}
	}

	MenuItem
	{
		id: removeID
		text: qsTranslate("BookContextMenu", "Remove book")
        onTriggered: controller.Remove(menuID.bookId)
	}

	MenuItem
	{
		id: restoreID
		text: qsTranslate("BookContextMenu", "Undo book deletion")
        onTriggered: controller.Restore(menuID.bookId)
	}

	onAboutToShow:
	{
		selectAllID.enabled = !controller.AllSelected()
		deselectAllID.enabled = controller.HasSelected()
		removeID.visible = controller.RemoveAvailable(menuID.bookId)
		restoreID.visible = controller.RestoreAvailable(menuID.bookId)
	}
}
