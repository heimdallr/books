import QtQuick 2.15
import QtQuick.Controls 2.15

import "Book"
import "Core"
import "Navigation"
import "Tray"

ApplicationWindow
{
	id: applicationWindowID

	property alias focus: splitViewID.focus
	property bool completed: false

	width: uiSettings.mainWindowWidth
	height: uiSettings.mainWindowHeight

	title: guiController.title

	visible: true

	onWidthChanged: if (completed) uiSettings.mainWindowWidth = width
	onHeightChanged: if (completed) uiSettings.mainWindowHeight = height
	onXChanged: if (completed) uiSettings.mainWindowPosX = x
	onYChanged: if (completed) uiSettings.mainWindowPosY = y

	Component.onCompleted:
	{
		if ((x = uiSettings.mainWindowPosX) < 0)
			x = (screen.width - width) / 2
		if ((y = uiSettings.mainWindowPosY) < 0)
			y = (screen.height - height) / 2

		completed = true;
	}

	Tray {}

	Component
	{
		id: splitViewComponentID

		SplitView
		{
			focus: true
			Keys.onPressed: guiController.OnKeyPressed(event.key, event.modifiers)

			anchors.fill: parent
			orientation: Qt.Horizontal

			handle: SplitViewHandle {}

			Navigation
			{
				SplitView.minimumWidth: applicationWindowID.width / 6
				SplitView.maximumWidth: applicationWindowID.width / 2
				SplitView.preferredWidth: uiSettings.navigationWidth * applicationWindowID.width

				onWidthChanged: if (applicationWindowID.completed)
					uiSettings.navigationWidth = width / applicationWindowID.width
			}

			Books
			{
				SplitView.fillWidth: true
			}
		}
	}

	Loader
	{
		id: splitViewID
		anchors.fill: parent
		sourceComponent: guiController.opened ? splitViewComponentID : undefined
	}
}
