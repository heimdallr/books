import QtQuick 2.15
import QtQuick.Controls 2.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Rectangle
{
	id: expandableID
	property bool expanded
	property int treeMargin: 0
	property alias expanderVisible: expanderID.visible
	property alias checkboxVisible: checkBoxID.visible
	property alias checkboxState: checkBoxID.checkState
	property alias text: textID.text
	property alias textColor: textID.color

	signal clicked()
	signal expanderClicked()
	signal checkboxClicked()

	color: "transparent"
	border { color: Constants.borderColor; width: 1 }

	Expander
	{
		id: expanderID
		height: parent.height / 2
		width: height

		anchors
		{
			verticalCenter: parent.verticalCenter
			left: parent.left
			leftMargin: 4 + treeMargin
		}

		expanded: expandableID.expanded

		onClicked: () => expandableID.expanderClicked()
	}

	CustomCheckbox
	{
		id: checkBoxID
		anchors
		{
			verticalCenter: parent.verticalCenter
			left: expanderID.right
			leftMargin: 4
		}

		tristate: true
		visible: false

		onClicked:  () => expandableID.checkboxClicked()
	}

	CustomText
	{
		id: textID
		anchors
		{
			left: checkBoxID.visible ? checkBoxID.right : expanderID.right
			leftMargin: 4
			bottom: parent.bottom
			bottomMargin: 4
		}
	}

	MouseArea
	{
		anchors
		{
			left: textID.left
			top: parent.top
			bottom: parent.bottom
			right: parent.right
		}

		onClicked: expandableID.clicked()
	}
}
