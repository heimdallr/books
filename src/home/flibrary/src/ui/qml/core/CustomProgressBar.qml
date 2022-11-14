import QtQuick 2.15
import QtQuick.Controls 1.4

import "qrc:/Util/Functions.js" as Functions

Rectangle
{
	color: "transparent"
	border { width: 1; color: uiSettings.colorBorder }

	Rectangle
	{
		color: "transparent"

		anchors
		{
			left: parent.left
			top: parent.top
			bottom: parent.bottom
			right: buttonStopID.left
			rightMargin: 3 * Functions.GetMargin() / 2
		}

		Rectangle
		{
			anchors
			{
				left: parent.left
				top: parent.top
				bottom: parent.bottom
				margins: Functions.GetMargin()
			}

			color: uiSettings.colorProgressBar
			radius: 5

			width: parent.width * progressController.progress
		}

		CustomText
		{
			anchors.centerIn: parent
			text: Math.ceil(100 * progressController.progress) + " %"
		}
	}

	Button
	{
		id: buttonStopID

		anchors
		{
			right: parent.right
			top: parent.top
			bottom: parent.bottom
			margins: Functions.GetMargin()
		}

		width: Functions.GetMargin() * 4
		text: qsTranslate("Common", "Cancel")
		onClicked: progressController.started = false
	}
}
