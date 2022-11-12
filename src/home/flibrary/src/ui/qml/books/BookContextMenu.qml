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

	Menu
	{
		id: sendToDeviceMenuID
		title: qsTranslate("BookContextMenu", "Send to device")

		function save(archivate)
		{
			const folder = fileDialog.SelectFolder(uiSettings.recentExportPath)
			if (folder === "")
				return

			uiSettings.recentExportPath = folder
			archivate
				? controller.WriteToArchive(folder, menuID.bookId)
				: controller.WriteToFile(folder, menuID.bookId)
		}

		MenuItem
		{
			text: qsTranslate("BookContextMenu", "In zip archive")
			onTriggered: sendToDeviceMenuID.save(true)
		}

		MenuItem
		{
			text: qsTranslate("BookContextMenu", "In original format")
			onTriggered: sendToDeviceMenuID.save(false)
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
