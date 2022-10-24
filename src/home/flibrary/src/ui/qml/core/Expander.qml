import QtQuick 2.15

import "qrc:/Core/constants.js" as Constants

Rectangle
{
	id: expanderID
	property bool expanded

	signal clicked()

	Rectangle
	{
		anchors
		{
			verticalCenter: parent.verticalCenter
			horizontalCenter: parent.horizontalCenter
		}
		width: parent.width / 2
		height: 2
		border { color: Constants.borderColor; width: 1 }
	}

	Rectangle
	{
		anchors
		{
			verticalCenter: parent.verticalCenter
			horizontalCenter: parent.horizontalCenter
		}
		width: 2
		height: parent.height / 2
		border { color: Constants.borderColor; width: 1 }
		visible: !expanded
	}

	color: "transparent"
	radius: 5
	border { color: Constants.borderColor; width: 1 }

	MouseArea
	{
		anchors.fill: parent
		onClicked: () => expanderID.clicked()
	}
}
