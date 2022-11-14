import QtQuick 2.15

import "qrc:/Core"

Item
{
	id: booksViewID

	BooksHeader
	{
		id: headerID

		height: uiSettings.heightRow
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
			height: uiSettings.heightRow
			width: booksViewID.width
		}
	}

	Component.onCompleted: viewTemplateID.modelController = guiController.GetBooksModelControllerTree()
}
