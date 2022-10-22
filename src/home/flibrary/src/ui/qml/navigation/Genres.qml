import QtQuick 2.15

import "qrc:/Core"

Item
{
	id: genresID
	CustomListView
	{
		anchors.fill: parent

		delegate: NavigationTreeDelegate
		{
			width: genresID.width
		}
	}

	Component.onCompleted: viewTemplateID.modelController = guiController.GetNavigationModelControllerGenres()
}
