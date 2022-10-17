import QtQuick 2.15

import "Core"

CustomListView
{
	id: listViewID
	modelController: guiController.GetBooksModelController()
	delegate: AuthorDelegate
	{
		itemWidth: listViewID.width
	}
}
