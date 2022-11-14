import QtQuick 2.15

import "qrc:/Core"
import "qrc:/Util/Functions.js" as Functions

Item
{
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
			leftMargin: Functions.GetMargin()
			bottom: parent.bottom
			bottomMargin: Functions.GetMargin()
		}

		text: Title
	}

	MouseArea
	{
		anchors.fill: parent
		onClicked: Click = true
	}
}
