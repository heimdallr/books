import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import "qrc:/Core"

Item
{
	id: delegateID
	property bool authorsVisible: true
	property bool seriesVisible: true
	property bool genresVisible: true

	Component
	{
		id: dictionaryID
		Expandable
		{
			height: delegateID.height

			text: Title
			textColor: IsDeleted ? "gray" : "black"
			expanded: Expanded
			treeMargin: height * TreeLevel / 2

			expanderVisible: true
			onExpanderClicked: Expanded = !expanded

			checkboxVisible: true
			checkboxState: Checked
			onCheckboxClicked: Checked = true

			onClicked: Click = true
		}
	}

	Component
	{
		id: bookID
		BookLayout
		{
			authorsVisible: delegateID.authorsVisible
			seriesVisible: delegateID.seriesVisible
			genresVisible: delegateID.genresVisible
			treeMargin: height * (TreeLevel + 1) / 2
		}
	}

	Loader
	{
		anchors.fill: parent
		sourceComponent: IsDictionary ? dictionaryID : bookID
	}
}
