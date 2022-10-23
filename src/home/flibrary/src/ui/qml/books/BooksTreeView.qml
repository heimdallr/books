import QtQuick 2.15

import "qrc:/Core"

Item
{
	id: booksListViewID

	readonly property bool genresVisible: guiController.genresVisible

	CustomListView
	{
		anchors.fill: parent

		delegate: BookTreeDelegate
		{
			width: booksListViewID.width
			authorsVisible: false
			seriesVisible: false
			genresVisible: booksListViewID.genresVisible
		}
	}

	Component.onCompleted: viewTemplateID.modelController = guiController.GetBooksModelControllerTree()
}
