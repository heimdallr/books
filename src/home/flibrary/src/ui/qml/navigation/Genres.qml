import QtQuick 2.15

import "qrc:/Core"

Item
{
	id: genresID
	CustomListView
	{
		anchors.fill: parent

		modelType: "Genres"
		delegate: NavigationDelegate
		{
			width: genresID.width
		}
	}
}
