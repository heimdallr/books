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
			width: seriesID.width
		}
	}

	Component.onCompleted: viewTemplateID.modelController = guiController.GetNavigationModelControllerSeries()
}
