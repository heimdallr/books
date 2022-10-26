import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Item
{
	id: booksListViewID

	readonly property bool authorsVisible: guiController.authorsVisible
	readonly property bool seriesVisible: guiController.seriesVisible
	readonly property bool genresVisible: guiController.genresVisible

	SplitView
	{
		id: headerID
		anchors
		{
			left: parent.left
			right: parent.right
			top: parent.top
		}

		height: Constants.delegateHeight

		orientation: Qt.Horizontal
		handle: SplitViewHandle {}

		CustomText
		{
			id: authorID
			visible: booksListViewID.authorsVisible
			text: qsTranslate("Header", "Author")
		}

		CustomText
		{
			id: seriesID
			visible: booksListViewID.seriesVisible
			text: qsTranslate("Header", "SeriesTitle")
		}

		CustomText
		{
			text: qsTranslate("Header", "Title")
		}

		CustomText
		{
			id: genreID
			visible: booksListViewID.genresVisible
			text: qsTranslate("Header", "GenreAlias")
		}

		CustomText
		{
			text: qsTranslate("Header", "Lang")
		}
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
			width: booksListViewID.width
			authorsVisible: booksListViewID.authorsVisible
			seriesVisible: booksListViewID.seriesVisible
			genresVisible: booksListViewID.genresVisible
		}
	}

	Component.onCompleted: viewTemplateID.modelController = guiController.GetBooksModelControllerList()
}
