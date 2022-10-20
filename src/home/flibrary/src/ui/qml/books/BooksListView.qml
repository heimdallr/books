import QtQuick 2.15

import "qrc:/Core"

Item
{
	id: booksListViewID

	readonly property bool authorVisible: modelController.navigationType != "Authors"

	CustomListView
	{
		anchors.fill: parent

		modelType: "BooksListView"
		delegate: BookDelegate
		{
			width: booksListViewID.width
			authorVisible: booksListViewID.authorVisible
		}
	}
}
