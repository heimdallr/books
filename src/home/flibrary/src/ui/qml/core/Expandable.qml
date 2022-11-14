import QtQuick 2.15
import QtQuick.Controls 2.15

import "qrc:/Core"
import "qrc:/Util/Functions.js" as Functions

Item
{
	id: expandableID
	property bool expanded
	property int treeMargin: 0
	property alias expanderVisible: expanderID.visible
	property alias checkboxVisible: checkBoxID.visible
	property alias checkboxState: checkBoxID.checkedState
	property alias text: textID.text
	property alias textColor: textID.color

	signal clicked()
	signal expanderClicked()
	signal checkboxClicked()

	TopBottomLines
	{
		anchors.fill: parent
	}

	Expander
	{
		id: expanderID
		height: parent.height * uiSettings.sizeExpander
		width: height

		anchors
		{
			verticalCenter: parent.verticalCenter
			left: parent.left
			leftMargin: Functions.GetMargin() + treeMargin
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
			leftMargin: Functions.GetMargin()
		}

		visible: false

		onClicked:  () => expandableID.checkboxClicked()
	}

	CustomText
	{
		id: textID
		anchors
		{
			left: checkBoxID.visible ? checkBoxID.right : expanderID.right
			right: parent.right
			leftMargin: Functions.GetMargin() * 2
			bottom: parent.bottom
			bottomMargin: Functions.GetMargin()
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
