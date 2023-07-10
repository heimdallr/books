import QtQuick 2.15

import "qrc:/Core"

Item
{
	id: groupsID
	CustomListView
	{
		anchors.fill: parent

		delegate: NavigationListDelegate
		{
			height: uiSettings.heightRow
			width: groupsID.width
		}
	}
}
