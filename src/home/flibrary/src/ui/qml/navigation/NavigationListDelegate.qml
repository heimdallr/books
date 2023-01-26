import QtQuick 2.15
import QtQuick.Controls 1.4

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
		anchors.fill: parent
		text: Title
	}

	GroupsContextMenu
	{
		id: groupsContextMenuID
	}

	MouseArea
	{
		anchors.fill: parent
		acceptedButtons: Qt.LeftButton | Qt.RightButton
		onClicked: (mouse)=>
		{
			if (mouse.button == Qt.LeftButton)
			{
				Click = true
			}
			else if (uiSettings.viewSourceNavigation == "Groups")
			{
				groupsContextMenuID.navigationId = Id
				groupsContextMenuID.popup()
			}
		}
	}
}
