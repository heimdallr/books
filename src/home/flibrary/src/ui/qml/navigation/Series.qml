import QtQuick 2.15

import "qrc:/Core"

Item
{
	id: seriesID
	CustomListView
	{
		anchors.fill: parent

		modelType: "Series"
		delegate: NavigationDelegate
		{
			itemWidth: seriesID.width
		}
	}
}
