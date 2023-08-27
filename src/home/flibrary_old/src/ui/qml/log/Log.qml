import QtQuick 2.15
import QtQuick.Controls 2.15

import Style 1.0

import "qrc:/Core"
import "qrc:/Tray"
import "qrc:/Util/Functions.js" as Functions

Rectangle
{
	color: uiSettings.colorBackground

	ListView
	{
		id: logViewID

		readonly property alias scrollBarWidth: scrollBarID.width
		readonly property int fontPointSize: uiSettings.sizeFont

		anchors
		{
			fill: parent
			margins: Functions.GetMargin()
		}

		model: log.GetLogModel()

		clip: true
		flickableDirection: Flickable.VerticalFlick

		ScrollBar.vertical: ScrollBar { id: scrollBarID; width: guiController.GetPixelMetric(Style.PM_ScrollBarExtent) * uiSettings.sizeScrollbar }
		delegate: Text
		{
			font.pointSize: logViewID.fontPointSize
			wrapMode: Text.Wrap
			width: logViewID.width
			text: Message
			color: Color
		}

		onCountChanged: Qt.callLater( function(){ scrollBarID.position = 1.0 - scrollBarID.size } )
		Component.onCompleted: logViewID.positionViewAtEnd()
	}

	MouseArea
	{
		LogContextMenu
		{
			id: logContextMenuID
		}

		anchors
		{
			fill: parent
			rightMargin: Functions.GetMargin() + logViewID.scrollBarWidth
		}

		acceptedButtons: Qt.LeftButton | Qt.RightButton
		onClicked:
		(mouse) =>
		{
			if (mouse.button == Qt.RightButton)
				logContextMenuID.popup()
		}
	}
}