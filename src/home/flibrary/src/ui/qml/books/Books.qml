import QtQuick 2.15
import QtQuick.Controls 2.15

import "Core"

Rectangle
{
	id: booksID

	SplitView
	{
		id: splitViewID

		anchors.fill: parent
		orientation: Qt.Vertical

		handle: SplitViewHandle {}

		BooksList
		{
			id: booksListID

			SplitView.preferredHeight: 3 * booksID.height / 4
			SplitView.minimumHeight: booksID.height / 4
			SplitView.maximumHeight: 7 * booksID.height / 8
		}

		BookInfo
		{
			id: bookInfoID
			SplitView.fillHeight: true
		}
	}
}
