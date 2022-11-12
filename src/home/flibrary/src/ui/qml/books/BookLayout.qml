import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import "qrc:/Core"

Item
{
	id: bookLayoutID

	property alias authorsVisible: authorID.visible
	property alias seriesVisible: seriesID.visible
	property alias genresVisible: genreID.visible
	property int treeMargin: 0

	property color textColor: IsDeleted ? "gray" : "black"

	TopBottomLines
	{
		anchors.fill: parent
	}

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

		checkedState: Checked
		onClicked: Checked = true
	}

	Item
	{
		anchors { left: checkBoxID.right; top: parent.top; bottom: parent.bottom; right: parent.right }

		RowLayout
		{
			id: layoutID
			anchors.fill: parent

			CustomText
			{
				id: authorID
				Layout.preferredWidth: bookLayoutID.width * uiSettings.authorWidth - uiSettings.splitViewHandleSize
				Layout.fillWidth: visible
				visible: delegateID.authorVisible
				color: textColor
				text: Author
//				onWidthChanged:	console.log(`bool author width: ${width}`)
			}

			CustomText
			{
				Layout.preferredWidth: bookLayoutID.width * uiSettings.titleWidth - uiSettings.splitViewHandleSize
				Layout.fillWidth: !authorID.visible
				color: textColor
				text: Title
//				onWidthChanged:	console.log(`bool title width: ${width}`)
			}

			CustomText
			{
				id: seriesID
				Layout.preferredWidth: bookLayoutID.width * uiSettings.seriesWidth - uiSettings.splitViewHandleSize
				visible: delegateID.seriesVisible
				color: textColor
				text: SeriesTitle
//				onWidthChanged:	console.log(`bool series width: ${width}`)
			}

			CustomText
			{
				readonly property int seqNumber: SeqNumber
				Layout.preferredWidth: uiSettings.seqNoWidth - uiSettings.splitViewHandleSize
				color: textColor
				text: seqNumber > 0 ? seqNumber : ""
//				onWidthChanged:	console.log(`bool seqNo width: ${width}`)
			}

			CustomText
			{
				id: genreID
				Layout.preferredWidth: bookLayoutID.width * uiSettings.genreWidth - uiSettings.splitViewHandleSize
				color: textColor
				text: GenreAlias
//				onWidthChanged:	console.log(`bool genre width: ${width}`)
			}

			CustomText
			{
				Layout.preferredWidth: uiSettings.languageWidth - uiSettings.splitViewHandleSize
				color: textColor
				text: Lang
//				onWidthChanged:	console.log(`bool language width: ${width}`)
			}
		}

		MouseArea
		{
			BookContextMenu
			{
				id: bookContextMenuID
			}

			anchors.fill: parent
			acceptedButtons: Qt.LeftButton | Qt.RightButton
			onClicked: (mouse)=>
			{
				if (mouse.button == Qt.LeftButton)
				{
					Click = true
				}
				else
				{
					bookContextMenuID.bookId = Id
					bookContextMenuID.popup()
				}
			}
		}
	}
}
