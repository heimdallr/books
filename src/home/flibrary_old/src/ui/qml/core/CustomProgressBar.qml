import QtQuick 2.15
import QtQuick.Controls 2.15

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
			height: parent.height
			width: 2 * preferredWidth
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

	    contentItem: Text
		{
	        text: buttonStopID.text
	        font.pointSize: uiSettings.sizeFont * uiSettings.sizeFontButton
	        opacity: enabled ? 1.0 : 0.3
	        horizontalAlignment: Text.AlignHCenter
	        verticalAlignment: Text.AlignVCenter
	        elide: Text.ElideRight
	    }

		width: height * 8
		text: qsTranslate("Common", "Cancel")
		onClicked: progressController.started = false
	}
}
