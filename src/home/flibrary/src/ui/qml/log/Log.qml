import QtQuick 2.15
import QtQuick.Controls 2.15

import "qrc:/Core"
import "qrc:/Tray"
import "qrc:/Util/Functions.js" as Functions

Rectangle
{
	color: uiSettings.colorBackground

	ListView
	{
		id: logViewID

		anchors
		{
			fill: parent
			margins: Functions.GetMargin()
		}

		model: logController.GetLogModel()

		clip: true
		flickableDirection: Flickable.VerticalFlick

		ScrollBar.vertical: ScrollBar { id: scrollBarID }
		delegate: Item
		{
			height: uiSettings.heightRow
			width: logViewID.width
			CustomText
			{
				text: Message
				color: Color
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

		anchors.fill: parent
		acceptedButtons: Qt.LeftButton | Qt.RightButton
		onClicked:
		(mouse) =>
		{
			if (mouse.button == Qt.RightButton)
				logContextMenuID.popup()
		}
	}
}
