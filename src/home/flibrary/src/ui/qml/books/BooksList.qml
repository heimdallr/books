import QtQuick 2.15

import "qrc:/ViewTemplate"
import "qrc:/Core"

Item
{
	ViewTemplate
	{
		id: listViewID
		anchors
		{
			left: parent.left
			right: parent.right
			top: parent.top
			bottom: progressBarID.top
		}

		loadPath: "qrc:/Book/"
		viewSourceComboBoxController: guiController.GetViewSourceComboBoxBooksController()
	}

	CustomProgressBar
	{
		id: progressBarID
		anchors
		{
			left: parent.left
			right: parent.right
			bottom: parent.bottom
		}

		visible: progressController.started
		height: visible ? uiSettings.heightRow * 3 / 2 : 0
	}
}