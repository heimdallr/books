import QtQuick 2.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Item
{
	id: authorsID
	CustomListView
	{
		anchors.fill: parent

		delegate: NavigationListDelegate
		{
			height: Constants.delegateHeight
			width: authorsID.width
		}
	}

	Component.onCompleted:
	{
		viewTemplateID.modelController = guiController.GetNavigationModelControllerAuthors()
		viewTemplateID.modelController.currentIndex = -1
	}
}
