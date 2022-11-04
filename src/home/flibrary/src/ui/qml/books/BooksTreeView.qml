import QtQuick 2.15

import "qrc:/Core"

Item
{
	id: booksViewID

	readonly property bool authorsVisible: guiController.authorsVisible
	readonly property bool seriesVisible: guiController.seriesVisible
	readonly property bool genresVisible: guiController.genresVisible

	BooksHeader
	{
		id: headerID
		anchors
		{
			left: parent.left
			right: parent.right
			top: parent.top
		}

		authorsVisible: booksViewID.authorsVisible
		seriesVisible: booksViewID.seriesVisible
		genresVisible: booksViewID.genresVisible
	}

	CustomListView
	{
		anchors
		{
			left: parent.left
			right: parent.right
			top: headerID.bottom
			bottom: parent.bottom
		}

		delegate: BookTreeDelegate
		{
			width: booksViewID.width
			authorsVisible: booksViewID.authorsVisible
			seriesVisible: booksViewID.seriesVisible
			genresVisible: booksViewID.genresVisible
		}
	}

	Component.onCompleted: viewTemplateID.modelController = guiController.GetBooksModelControllerTree()
}
