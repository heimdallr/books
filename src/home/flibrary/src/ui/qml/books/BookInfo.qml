import QtQuick 2.15
import QtQuick.Controls 2.15

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
			selectByMouse: true
			wrapMode: TextEdit.WordWrap
			font.pointSize: uiSettings.sizeFont
			text: annotationController.annotation
	    }
	}
}
