import QtQuick 2.15

import "qrc:/Core/constants.js" as Constants

Rectangle
{
	height: Constants.delegateHeight
	color: "transparent"

	border { color: Constants.borderColor; width: 1 }

	Text
	{
		id: textID
		anchors
		{
			left: parent.left
			leftMargin: 4
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
