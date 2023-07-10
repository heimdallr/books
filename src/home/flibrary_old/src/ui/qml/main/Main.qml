import QtQuick 2.15
import QtQuick.Controls 2.15

import "qrc:/Log"
import "qrc:/Dialogs"

ApplicationWindow
{
	id: applicationWindowID

	property bool completed: false

	width: uiSettings.widthMainWindow
	height: uiSettings.heightMainWindow

	title: guiController.title
	color: uiSettings.colorBackground

	visible: true

	onWidthChanged: if (completed) uiSettings.widthMainWindow = width
	onHeightChanged: if (completed) uiSettings.heightMainWindow = height
	onXChanged: if (completed) uiSettings.posXMainWindow = x
	onYChanged: if (completed) uiSettings.posYMainWindow = y

	function showNavigationContextMenu(id)
	{
		contextMenuID.source = ""
		switch(uiSettings.viewSourceNavigation)
		{
			case "Groups":
				guiController.GetGroupsModelController().SetCurrentId(id)
				contextMenuID.source = "qrc:/Navigation/GroupsContextMenu.qml"
			break
		}
	}

	Loader
	{
		id: contextMenuID
		onStatusChanged: if (status == Loader.Ready)
			item.popup()
	}

	AddCollection
	{
		id: addCollectionID
		visible: collectionController.GetAddModeDialogController().visible
	}

	Loader
	{
		id: splitViewID
		anchors.fill: parent
		source: guiController.opened ? "qrc:/Main/LibraryView.qml" : ""
	}

	Log
	{
		anchors.fill: parent
		visible: log.logMode
	}

	Component.onCompleted:
	{
		if ((x = uiSettings.posXMainWindow) < 0)
			x = (screen.width - width) / 2
		if ((y = uiSettings.posYMainWindow) < 0)
			y = (screen.height - height) / 2

		completed = true;

		log.Debug(`ApplicationWindow created`)
	}
}
