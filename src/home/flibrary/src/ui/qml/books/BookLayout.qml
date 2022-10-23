import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Item
{
	id: itemID
	property alias authorsVisible: authorID.visible
	property alias seriesVisible: seriesID.visible
	property alias genresVisible: genreID.visible

	RowLayout
	{
		id: layoutID
		anchors.fill: parent

		CheckBox
		{
			id: checkBoxID
			checked: Checked
			onClicked: Checked = !Checked
		}

		CustomText
		{
			id: authorID
			Layout.preferredWidth: layoutID.width / 8
			visible: delegateID.authorVisible
			text: Author
		}

		CustomText
		{
			id: seriesID
			Layout.preferredWidth: layoutID.width / 16
			visible: delegateID.seriesVisible
			text: SeriesTitle
		}

		CustomText
		{
			Layout.fillWidth: true
			text: Title
		}

		CustomText
		{
			id: genreID
			Layout.preferredWidth: layoutID.width / 8
			text: GenreAlias
		}

		CustomText
		{
			Layout.preferredWidth: 36
			text: Lang
		}
	}

	MouseArea
	{
		anchors.fill: parent
		anchors.leftMargin: checkBoxID.width
		onClicked: Click = true
	}
}
