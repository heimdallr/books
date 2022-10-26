import QtQuick 2.15
import QtQuick.Controls 2.15

import "Navigation"
import "Book"
import "Core"

ApplicationWindow
{
	id: applicationWindowID

//	property alias focus: splitViewID.focus

	width: 1280
	height: 720

	title: qsTranslate("Main", "Flibrary - flibusta books collection")

	visible: true

	readonly property bool running: guiController.running

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
			id: splitViewID
			focus: true
			Keys.onPressed: guiController.OnKeyPressed(event.key, event.modifiers)

			anchors.fill: parent
			orientation: Qt.Horizontal

			handle: SplitViewHandle {}

			Navigation
			{
				SplitView.preferredWidth: applicationWindowID.width / 4
				SplitView.minimumWidth: applicationWindowID.width / 6
				SplitView.maximumWidth: applicationWindowID.width / 2
			}

			Books
			{
				SplitView.fillWidth: true
			}
		}
	}

	Loader
	{
		anchors.fill: parent
		sourceComponent: guiController.opened ? splitViewComponentID : undefined
	}
}
