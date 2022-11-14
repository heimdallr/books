import QtQuick 2.15

import "qrc:/Core"

Item
{
	id: authorsID
	CustomListView
	{
		anchors.fill: parent

		delegate: NavigationListDelegate
		{
			height: uiSettings.heightRow
			width: authorsID.width
		}
	}

	Component.onCompleted:
	{
		viewTemplateID.modelController = guiController.GetNavigationModelControllerAuthors()
		viewTemplateID.modelController.currentIndex = -1
	}
}
