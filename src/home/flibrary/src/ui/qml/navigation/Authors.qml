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
			itemWidth: authorsID.width
		}
	}
}
