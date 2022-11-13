import QtQuick 2.15
import QtQuick.Controls 1.4

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
			rightMargin: 6
		}

		Rectangle
		{
			anchors
			{
				left: parent.left
				top: parent.top
				bottom: parent.bottom
				margins: 4
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
			margins: 4
		}

		width: 80
		text: qsTranslate("Common", "Cancel")
		onClicked: progressController.started = false
	}
}
