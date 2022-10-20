import QtQuick 2.15

import "qrc:/Core"

Item
{
	id: booksListViewID
	CustomListView
	{
		anchors.fill: parent

		modelType: "BooksListView"
		delegate: BookDelegate
		{
			itemWidth: booksListViewID.width
		}
	}
}
