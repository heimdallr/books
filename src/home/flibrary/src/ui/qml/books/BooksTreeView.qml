import QtQuick 2.15

import "qrc:/Core"

Item
{
	id: booksViewID

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
		}
	}

	Component.onCompleted: viewTemplateID.modelController = guiController.GetBooksModelControllerTree()
}
