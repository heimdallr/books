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

	width:
	{
		const v =  uiSettings.mainWindowWidth
		return v > 0 ? v : 1024
	}

	height:
	{
		const v = uiSettings.mainWindowHeight
		return v > 0 ? v : 720
	}

	title: guiController.title

	visible: true

	readonly property bool running: guiController.running

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

	Menu
	{
		id: trayMenu

		MenuItem
		{
			text: qsTranslate("Tray", "Show Flibrary")
			onTriggered: application.show()
		}

		MenuItem
		{
			text: qsTranslate("Tray", "Exit")
			onTriggered:
			{
				systemTray.hide()
				Qt.quit()
			}
		}
	}

	onRunningChanged: if (!running)
		Qt.quit()

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

				onWidthChanged: if (applicationWindowID.completed)
					uiSettings.navigationWidth = width / applicationWindowID.width

				Component.onCompleted:
				{
					SplitView.preferredWidth = uiSettings.navigationWidth * applicationWindowID.width
				}
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
