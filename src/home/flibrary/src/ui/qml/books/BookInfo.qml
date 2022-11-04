import QtQuick 2.15
import QtQuick.Controls 2.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Rectangle
{
	readonly property var controller: guiController.GetAnnotationController()

	Image
	{
		id: imageID

		anchors
		{
			left: parent.left
			top: parent.top
			bottom: parent.bottom
		}

		fillMode: Image.PreserveAspectFit
		visible: controller.hasCover
		source: visible ? controller.cover : ""

		MouseArea
		{
			anchors
			{
				left: parent.left
				top: parent.top
				bottom: parent.bottom
			}

			width: parent.width / 2
			onClicked: controller.CoverPrev()
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
			onClicked: controller.CoverNext()
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
			wrapMode: TextEdit.WordWrap
			font.pointSize: Constants.fontSize
			text: controller.annotation
	    }
	}
}
