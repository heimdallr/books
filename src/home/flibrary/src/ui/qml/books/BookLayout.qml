import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Item
{
	property alias authorsVisible: authorID.visible
	property alias seriesVisible: seriesID.visible
	property alias genresVisible: genreID.visible
	property int treeMargin: 0

	Item
	{
		id: treeMarginID
		anchors{ left: parent.left; top: parent.top }
		width: treeMargin
		height: parent.height
	}

	CustomCheckbox
	{
		id: checkBoxID
		anchors
		{
			verticalCenter: parent.verticalCenter
			left: treeMarginID.right
		}

		checked: Checked
		onClicked: Checked = !Checked
	}

	Item
	{
		anchors{ left: checkBoxID.right; top: parent.top; bottom: parent.bottom; right: parent.right }

		RowLayout
		{
			id: layoutID
			anchors.fill: parent

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
			onClicked: Click = true
		}
	}
}
