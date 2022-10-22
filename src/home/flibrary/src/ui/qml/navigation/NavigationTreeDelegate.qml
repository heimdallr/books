import QtQuick 2.15

import "qrc:/Core/constants.js" as Constants

Rectangle
{
	height: Constants.delegateHeight
	color: "transparent"

	border { color: Constants.borderColor; width: 1 }

	Rectangle
	{
		id: plusID
		readonly property int size: parent.height / 2

		anchors
		{
			verticalCenter: parent.verticalCenter
			left: parent.left
			leftMargin: 4 + parent.height * TreeLevel
		}

		Rectangle
		{
			anchors
			{
				verticalCenter: parent.verticalCenter
				horizontalCenter: parent.horizontalCenter
			}
			width: plusID.size / 2
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
			height: plusID.size / 2
			border { color: Constants.borderColor; width: 1 }
		}

		color: "transparent"
		width: size
		height: size
		radius: 5
		border { color: Constants.borderColor; width: 1 }
		visible: ChildrenCount > 0
	}

	Text
	{
		id: textID
		anchors
		{
			left: parent.left
			leftMargin: 4 + 4 + plusID.size + parent.height * TreeLevel
			bottom: parent.bottom
			bottomMargin: 4
		}
		font.pointSize: Constants.fontSize

		color: "black"
		text: Title
	}

	MouseArea
	{
		anchors.fill: parent
		onClicked: Click = true
	}
}
