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

		anchors
		{
			fill: parent
			margins: Functions.GetMargin()
		}

		model: logController.GetLogModel()

		clip: true
		flickableDirection: Flickable.VerticalFlick

		ScrollBar.vertical: ScrollBar { id: scrollBarID; width: guiController.GetPixelMetric(Style.PM_ScrollBarExtent) * uiSettings.sizeScrollbar }
		delegate: Item
		{
			height: childrenRect.height
			width: logViewID.width
			CustomText
			{
				text: Message
				color: Color
				wrapMode: Text.Wrap
			}
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
