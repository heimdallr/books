import QtQuick 2.15

import "qrc:/Util/Functions.js" as Functions

Item
{
	id: customTextID

	property alias text: textID.text
	property alias font: textID.font
	property alias color: textID.color
	property alias wrapMode: textID.wrapMode
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
	}

	TextMetrics
	{
		id: textMetricsID
		font: textID.font
		text: textID.text
	}
}
