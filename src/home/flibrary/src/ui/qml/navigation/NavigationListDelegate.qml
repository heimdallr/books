import QtQuick 2.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Item
{
	height: Constants.delegateHeight

	TopBottomLines
	{
		anchors.fill: parent
	}

	CustomText
	{
		id: textID
		anchors
		{
			left: parent.left
			leftMargin: 4
			bottom: parent.bottom
			bottomMargin: 4
		}

		text: Title
	}

	MouseArea
	{
		anchors.fill: parent
		onClicked: Click = true
	}
}
