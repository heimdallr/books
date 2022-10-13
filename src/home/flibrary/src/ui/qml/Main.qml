import QtQuick 2.15
import QtQuick.Controls 2.15

ApplicationWindow
{
	id: applicationWindowID

	width: 800
	height: 600

	title: qsTranslate("Main", "Flibrary - flibusta books collection")

	visible: true

	SplitView
	{
		id: splitViewID

		anchors.fill: parent
		orientation: Qt.Horizontal

		handle: SplitViewHandle {}

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
