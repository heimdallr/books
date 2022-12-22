import QtQuick 2.15

import "qrc:/Core"

Item
{
	id: genresID
	CustomListView
	{
		anchors.fill: parent

		delegate: NavigationTreeDelegate
		{
			height: uiSettings.heightRow
			width: genresID.width
		}
	}
}
