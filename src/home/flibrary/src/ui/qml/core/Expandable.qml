import QtQuick 2.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Rectangle
{
	id: expandableID
	property bool expanded
	property int level
	property alias expanderVisible: expanderID.visible

	signal clicked()
	signal expanderClicked()

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
			leftMargin: 4 + width * level
		}

		expanded: expandableID.expanded

		onClick: () => expandableID.expanderClicked()
	}

	CustomText
	{
		id: textID
		anchors
		{
			left: expanderID.right
			leftMargin: 4
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

		onClicked: expandableID.clicked()
	}
}
