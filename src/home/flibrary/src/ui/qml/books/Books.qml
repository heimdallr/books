import QtQuick 2.15
import QtQuick.Controls 2.15

import "qrc:/Core"

Rectangle
{
	id: booksID

	property bool ready: false

	SplitView
	{
		id: splitViewID

		anchors.fill: parent
		orientation: Qt.Vertical

		handle: SplitViewHandle {}

		BooksList
		{
			id: booksListID

			SplitView.fillHeight: true
		}

		BookInfo
		{
			id: bookInfoID

			SplitView.minimumHeight: booksID.height / 8
			SplitView.maximumHeight: 3 * booksID.height / 4
			SplitView.preferredHeight: ready ? uiSettings.heightBookInfo * booksID.height : applicationWindowID.height / 4

			onHeightChanged: if (applicationWindowID.completed)
				uiSettings.heightBookInfo = height / booksID.height

			visible: uiSettings.showBookInfo != 0
		}
	}
	onHeightChanged:
	{
		if (ready)
			return
	}
}
