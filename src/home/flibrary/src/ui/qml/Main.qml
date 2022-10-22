import QtQuick 2.15
import QtQuick.Controls 2.15

import "Navigation"
import "Book"
import "Core"

ApplicationWindow
{
	id: applicationWindowID

	property alias focus: splitViewID.focus

	width: 1280
	height: 720

	title: qsTranslate("Main", "Flibrary - flibusta books collection")

	visible: true

	readonly property bool running: guiController.running

	onRunningChanged: if (!running)
		close()

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
