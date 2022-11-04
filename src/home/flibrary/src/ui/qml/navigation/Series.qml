import QtQuick 2.15

import "qrc:/Core"

Item
{
	id: seriesID
	CustomListView
	{
		anchors.fill: parent

		delegate: NavigationListDelegate
		{
			height: uiSettings.delegateHeight
			width: seriesID.width
		}
	}

	Component.onCompleted:
	{
		viewTemplateID.modelController = guiController.GetNavigationModelControllerSeries()
		viewTemplateID.modelController.currentIndex = -1
	}
}
