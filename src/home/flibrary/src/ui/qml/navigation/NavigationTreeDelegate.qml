import QtQuick 2.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Rectangle
{
	readonly property bool expanded: Expanded

	height: Constants.delegateHeight
	color: "transparent"
	border { color: Constants.borderColor; width: 1 }

	Expander
	{
		id: expanderID
		height: parent.height / 2
		width: height

		anchors
		{
			verticalCenter: parent.verticalCenter
			left: parent.left
			leftMargin: 4 + width * TreeLevel
		}

		visible: ChildrenCount > 0
		expanded: Expanded

		onClick: () => Expand = !expanded
	}

	CustomText
	{
		id: textID
		anchors
		{
			left: parent.left
			leftMargin: 4 + 4 + expanderID.width + expanderID.width * TreeLevel
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
