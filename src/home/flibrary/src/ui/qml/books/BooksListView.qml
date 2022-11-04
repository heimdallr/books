import QtQuick 2.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Item
{
	id: booksViewID

	readonly property bool authorsVisible: guiController.authorsVisible
	readonly property bool seriesVisible: guiController.seriesVisible
	readonly property bool genresVisible: guiController.genresVisible

	BooksHeader
	{
		id: headerID

		height: Constants.delegateHeight
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

		delegate: BookListDelegate
		{
			width: booksViewID.width
			height: Constants.delegateHeight

			authorsVisible: booksViewID.authorsVisible
			seriesVisible: booksViewID.seriesVisible
			genresVisible: booksViewID.genresVisible
		}
	}

	Component.onCompleted: viewTemplateID.modelController = guiController.GetBooksModelControllerList()
}
