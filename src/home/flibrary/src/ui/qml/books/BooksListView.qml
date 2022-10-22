import QtQuick 2.15

import "qrc:/Core"

Item
{
	id: booksListViewID

	readonly property bool authorsVisible: guiController.IsAuthorsVisible()
	readonly property bool seriesVisible: guiController.IsSeriesVisible()
	readonly property bool genresVisible: guiController.IsGenresVisible()

	CustomListView
	{
		anchors.fill: parent

		delegate: BookDelegate
		{
			width: booksListViewID.width
			authorsVisible: booksListViewID.authorsVisible
			seriesVisible: booksListViewID.seriesVisible
			genresVisible: booksListViewID.genresVisible
		}
	}

	Component.onCompleted: viewTemplateID.modelController = guiController.GetBooksModelController()
}
