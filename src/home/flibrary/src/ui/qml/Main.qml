import QtQuick 2.15
import QtQuick.Controls 2.15

ApplicationWindow
{
	id: applicationWindowID

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

		anchors.fill: parent
		orientation: Qt.Horizontal

		handle: SplitViewHandle {}

		focus: guiController.mainWindowFocused

		Keys.onPressed: (event) => guiController.OnKeyPressed(event.key, event.modifiers)

		Navigation
		{
			id: navigationID

			SplitView.preferredWidth: applicationWindowID.width / 4
			SplitView.minimumWidth: applicationWindowID.width / 6
			SplitView.maximumWidth: applicationWindowID.width / 2
		}

		Books
		{
			id: booksID
			SplitView.fillWidth: true
		}
	}
}
