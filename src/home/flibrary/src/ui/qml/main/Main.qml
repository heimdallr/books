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

	Component.onCompleted:
	{
		if ((x = uiSettings.posXMainWindow) < 0)
			x = (screen.width - width) / 2
		if ((y = uiSettings.posYMainWindow) < 0)
			y = (screen.height - height) / 2

		completed = true;

		log.Debug(`ApplicationWindow created`)
	}

	AddCollection
	{
		id: addCollectionID
		visible: collectionController.addMode
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
}
