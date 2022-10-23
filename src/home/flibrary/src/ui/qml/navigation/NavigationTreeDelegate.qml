import QtQuick 2.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Rectangle
{
	readonly property bool expanded: Expanded

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
			leftMargin: 4 + size * TreeLevel
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
			visible: !expanded
		}

		color: "transparent"
		width: size
		height: size
		radius: 5
		border { color: Constants.borderColor; width: 1 }
		visible: ChildrenCount > 0

		MouseArea
		{
			anchors.fill: parent
			onClicked: Expand = !expanded
		}
	}

	CustomText
	{
		id: textID
		anchors
		{
			left: parent.left
			leftMargin: 4 + 4 + plusID.size + plusID.size * TreeLevel
			bottom: parent.bottom
			bottomMargin: 4
		}

		text: Title
	}

	MouseArea
	{
		anchors
		{
			left: textID.left
			top: parent.top
			bottom: parent.bottom
			right: parent.right
		}

		onClicked: Click = true
	}
}
