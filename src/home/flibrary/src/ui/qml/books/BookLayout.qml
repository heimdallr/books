import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import "qrc:/Core"
import "qrc:/Util/Functions.js" as Functions

Item
{
	id: bookLayoutID

	property int treeMargin: 0

	property color textColor: IsDeleted ? uiSettings.colorBookTextDeleted : uiSettings.colorBookText

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
		anchors { left: checkBoxID.right; leftMargin: uiSettings.heightRow / 5; top: parent.top; bottom: parent.bottom; right: parent.right }

		RowLayout
		{
			id: layoutID
			anchors.fill: parent

			CustomText
			{
				id: authorID
				function getIndex() { return uiSettings.indexAuthor }
				property bool ready: false
				Layout.preferredWidth: ready ? bookLayoutID.width * uiSettings.widthAuthor - uiSettings.sizeSplitViewHandle : -1
				visible: fieldsVisibilityProvider.authorsVisible
				color: textColor
				text: Author
			}

			CustomText
			{
				id: titleID
				function getIndex() { return uiSettings.indexTitle }
				property bool ready: false
				Layout.preferredWidth: ready ? bookLayoutID.width * uiSettings.widthTitle - uiSettings.sizeSplitViewHandle : -1
				color: textColor
				text: Title
			}

			CustomText
			{
				id: seriesID
				function getIndex() { return uiSettings.indexSeries }
				property bool ready: false
				Layout.preferredWidth: ready ? bookLayoutID.width * uiSettings.widthSeries - uiSettings.sizeSplitViewHandle : -1
				visible: fieldsVisibilityProvider.seriesVisible
				color: textColor
				text: SeriesTitle
			}

			CustomText
			{
				id: seqNoID
				function getIndex() { return uiSettings.indexSeqNo }
				property bool ready: false
				readonly property int seqNumber: SeqNumber
				Layout.preferredWidth: ready ? uiSettings.widthSeqNo - uiSettings.sizeSplitViewHandle : -1
				color: textColor
				text: seqNumber > 0 ? seqNumber : ""
			}

			CustomText
			{
				id: sizeID
				function getIndex() { return uiSettings.indexSize }
				property bool ready: false
				Layout.preferredWidth: ready ? bookLayoutID.width * uiSettings.widthSize - uiSettings.sizeSplitViewHandle : -1
				Layout.preferredHeight: bookLayoutID.height
				color: textColor
				text: measure.GetSize(Size)
			}

			CustomText
			{
				id: genreID
				function getIndex() { return uiSettings.indexGenre }
				property bool ready: false
				Layout.preferredWidth: ready ? bookLayoutID.width * uiSettings.widthGenre - uiSettings.sizeSplitViewHandle : -1
				visible: fieldsVisibilityProvider.genresVisible
				color: textColor
				text: GenreAlias
			}

			CustomText
			{
				id: langID
				function getIndex() { return uiSettings.indexLanguage }
				property bool ready: false
				Layout.preferredWidth: ready ? uiSettings.widthLanguage - uiSettings.sizeSplitViewHandle : -1
				color: textColor
				text: Lang
			}

			Component.onCompleted: Functions.SetWidths([authorID, titleID, seriesID, seqNoID, sizeID, genreID, langID], layoutID, function(item, value){ item.Layout.fillWidth = value })
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
