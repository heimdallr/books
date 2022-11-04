import QtQuick 2.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Item
{
	id: seriesID
	CustomListView
	{
		anchors.fill: parent

		delegate: NavigationListDelegate
		{
			height: Constants.delegateHeight
			width: seriesID.width
		}
	}

	Component.onCompleted:
	{
		viewTemplateID.modelController = guiController.GetNavigationModelControllerSeries()
		viewTemplateID.modelController.currentIndex = -1
	}
}
