import QtQuick 2.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Item
{
	id: genresID
	CustomListView
	{
		anchors.fill: parent

		delegate: NavigationTreeDelegate
		{
			height: Constants.delegateHeight
			width: genresID.width
		}
	}

	Component.onCompleted:
	{
		viewTemplateID.modelController = guiController.GetNavigationModelControllerGenres()
		viewTemplateID.modelController.currentIndex = -1
	}
}
