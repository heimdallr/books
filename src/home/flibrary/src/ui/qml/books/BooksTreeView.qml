import QtQuick 2.15

import "qrc:/Core"

Item
{
	id: booksViewID

	readonly property bool authorsVisible: fieldsVisibilityProvider.authorsVisible
	readonly property bool seriesVisible: fieldsVisibilityProvider.seriesVisible
	readonly property bool genresVisible: fieldsVisibilityProvider.genresVisible

	BooksHeader
	{
		id: headerID

		height: uiSettings.heightDelegate
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
			height: uiSettings.heightDelegate
			width: booksViewID.width

			authorsVisible: booksViewID.authorsVisible
			seriesVisible: booksViewID.seriesVisible
			genresVisible: booksViewID.genresVisible
		}
	}

	Component.onCompleted: viewTemplateID.modelController = guiController.GetBooksModelControllerTree()
}
