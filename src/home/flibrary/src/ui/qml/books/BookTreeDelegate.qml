import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Rectangle
{
	id: delegateID
	property bool authorsVisible: true
	property bool seriesVisible: true
	property bool genresVisible: true

	height: Constants.delegateHeight
	color: "transparent"

	border { color: Constants.borderColor; width: 1 }

	Component
	{
		id: dictionaryID
		Expandable
		{
			height: Constants.delegateHeight

			text: Title
			expanded: Expanded
			treeMargin: height * TreeLevel / 2

			expanderVisible: true
			onExpanderClicked: Expanded = !expanded

			checkboxVisible: true
			checkboxState: Qt.PartiallyChecked
			onCheckboxClicked: Checked = !Checked

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
