import QtQuick 2.15

import "qrc:/Core"

Item
{
	id: authorsID
	CustomListView
	{
		anchors.fill: parent

		delegate: NavigationDelegate
		{
			width: authorsID.width
		}
	}

	Component.onCompleted: viewTemplateID.modelController = guiController.GetNavigationModelControllerAuthors()
}
