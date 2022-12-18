import QtQuick 2.15
import QtQuick.Controls 2.15

import Style 1.0

import "qrc:/Core"

Rectangle
{
	Image
	{
		id: imageID

		anchors
		{
			left: parent.left
			top: parent.top
			bottom: parent.bottom
		}

		visible: annotationController.hasCover
		width: visible ? height * sourceSize.width / sourceSize.height : 0

		fillMode: Image.PreserveAspectFit
		source: visible ? annotationController.cover : ""

		MouseArea
		{
			anchors
			{
				left: parent.left
				top: parent.top
				bottom: parent.bottom
			}

			width: parent.width / 2
			onClicked: annotationController.CoverPrev()
		}

		MouseArea
		{
			anchors
			{
				right: parent.right
				top: parent.top
				bottom: parent.bottom
			}

			width: parent.width / 2
			onClicked: annotationController.CoverNext()
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
			onLinkActivated: (link) => { console.log(link) }

			MouseArea
			{
				anchors.fill: parent
				acceptedButtons: Qt.NoButton
				cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
			}
	    }
	}
}
