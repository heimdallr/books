import QtQuick 2.15

import "qrc:/Core"

Item
{
	id: booksListViewID

	readonly property bool authorsVisible: guiController.authorsVisible
	readonly property bool seriesVisible: guiController.seriesVisible
	readonly property bool genresVisible: guiController.genresVisible

	CustomListView
	{
		anchors.fill: parent

		delegate: BookListDelegate
		{
			width: booksListViewID.width
			authorsVisible: booksListViewID.authorsVisible
			seriesVisible: booksListViewID.seriesVisible
			genresVisible: booksListViewID.genresVisible
		}
	}

	Component.onCompleted: viewTemplateID.modelController = guiController.GetBooksModelControllerList()
}
