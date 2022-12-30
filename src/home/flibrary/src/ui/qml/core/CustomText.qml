import QtQuick 2.15
import QtQuick.Controls 2.15

import "qrc:/Util/Functions.js" as Functions

Item
{
	id: customTextID

	property alias text: textID.text
	property alias font: textID.font
	property alias color: textID.color
	property alias horizontalAlignment: textID.horizontalAlignment
	property int preferredWidth: textMetricsID.width + 2 * Functions.GetMargin()

	Text
	{
		id: textID
		elide: Text.ElideRight
		font.pointSize: uiSettings.sizeFont
		anchors
		{
			left: parent.left
			right: parent.right
			leftMargin: Functions.GetMargin()
			rightMargin: Functions.GetMargin()
			verticalCenter: parent.verticalCenter
		}

		ToolTip
		{
			delay: uiSettings.toolTipDelay
			timeout: uiSettings.toolTipTimeout
			visible: textID.truncated && mouseAreaID.containsMouse
			font.pointSize: uiSettings.sizeFont * uiSettings.sizeFontToolTip
			text: textID.text
		}
	}

	TextMetrics
	{
		id: textMetricsID
		font: textID.font
		text: textID.text
	}

	MouseArea
	{
		id: mouseAreaID
		anchors.fill: parent
		hoverEnabled: true
	}
}
