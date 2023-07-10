import QtQuick 2.15
import QtQuick.Controls 2.15

import Style 1.0

import "qrc:/Core"

Rectangle
{
	id: bookInfoID
	readonly property var annotationController: guiController.GetAnnotationController()
	readonly property int coverCount: annotationController.coverCount
	Image
	{
		id: imageID

		anchors
		{
			left: parent.left
			top: parent.top
		}

		visible: annotationController.hasCover
		height: visible ? Math.min(bookInfoID.height, bookInfoID.width * sourceSize.height / sourceSize.width / 2) : 0
		width: visible ? height * sourceSize.width / sourceSize.height : 0

		fillMode: Image.Stretch
		source: visible ? annotationController.cover : ""

		MouseArea
		{
			anchors
			{
				left: parent.left
				top: parent.top
				bottom: parent.bottom
			}

			width: parent.width / 3
			onClicked: annotationController.CoverPrev()
			cursorShape: bookInfoID.coverCount > 1 ? Qt.PointingHandCursor : Qt.ArrowCursor
		}

		MouseArea
		{
			anchors
			{
				right: parent.right
				top: parent.top
				bottom: parent.bottom
			}

			width: parent.width / 3
			onClicked: annotationController.CoverNext()
			cursorShape: bookInfoID.coverCount > 1 ? Qt.PointingHandCursor : Qt.ArrowCursor
		}
	}

	ScrollView
	{
		clip: true
		ScrollBar.vertical.width: guiController.GetPixelMetric(Style.PM_ScrollBarExtent) * uiSettings.sizeScrollbar

		anchors
		{
			left: imageID.right
			right: parent.right
			top: parent.top
			bottom: parent.bottom
		}

	    TextArea
		{
			readOnly: true
			selectByKeyboard: true
			textFormat: TextEdit.RichText
			selectByMouse: true
			wrapMode: TextEdit.WordWrap
			font.pointSize: uiSettings.sizeFont
			text: annotationController.annotation
			onLinkActivated: (link) => { guiController.HandleLink(link, annotationController.GetCurrentBookId()) }

			MouseArea
			{
				anchors.fill: parent
				acceptedButtons: Qt.NoButton
				cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
			}
	    }
	}
}
