import QtQuick 2.15
import QtQuick.Controls 1.4

Menu
{
	id: menuID

	readonly property var controller: guiController.GetBooksModelController()
	property int bookId: 0

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
		text: qsTranslate("BookContextMenu", "Restore book")
        onTriggered: controller.Restore(menuID.bookId)
	}

	onAboutToShow:
	{
		removeID.visible = controller.RemoveAvailable(menuID.bookId)
		restoreID.visible = controller.RestoreAvailable(menuID.bookId)
	}
}
