import QtQuick 2.15

import "qrc:/Core/constants.js" as Constants

Item
{
	Rectangle
	{
		height: 1
		color: Constants.borderColor
		anchors
		{
			left: parent.left
			right: parent.right
			top: parent.top
		}
	}

	Rectangle
	{
		height: 1
		color: Constants.borderColor
		anchors
		{
			left: parent.left
			right: parent.right
			bottom: parent.bottom
			bottomMargin: -1
		}
	}
}
