import QtQuick 2.15

import "qrc:/"
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

		loadPath: "Book/"
		Component.onCompleted:
		{
			viewSourceComboBox.add(qsTranslate("ViewSource", "List"), "BooksListView")
			viewSourceComboBox.add(qsTranslate("ViewSource", "Tree"), "BooksTreeView")
			viewSourceComboBox.currentIndex = 0
		}
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
		height: visible ? uiSettings.heightDelegate * 3 / 2 : 0
	}
}