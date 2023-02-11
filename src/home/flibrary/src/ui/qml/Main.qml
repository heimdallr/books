import QtQuick 2.15
import QtQuick.Controls 2.15

import "qrc:/Book"
import "qrc:/Core"
import "qrc:/Log"
import "qrc:/Navigation"
import "qrc:/Tray"

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

	Component
	{
		id: splitViewComponentID

		Item
		{
			Tray {}

			SplitView
			{
				anchors.fill: parent
				orientation: Qt.Horizontal

				handle: SplitViewHandle {}

				Navigation
				{
					SplitView.minimumWidth: applicationWindowID.width / 6
					SplitView.maximumWidth: applicationWindowID.width / 2
					SplitView.preferredWidth: uiSettings.widthNavigation * applicationWindowID.width

					onWidthChanged: if (applicationWindowID.completed)
						uiSettings.widthNavigation = width / applicationWindowID.width
				}

				Books
				{
					SplitView.fillWidth: true
				}
			}
		}
	}

	Loader
	{
		id: splitViewID
		anchors.fill: parent
		sourceComponent: guiController.opened ? splitViewComponentID : undefined
	}

	Log
	{
		anchors.fill: parent
		visible: log.logMode
	}
}
