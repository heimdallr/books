import QtQuick 2.15
import QtQuick.Controls 1.4

import "qrc:/Core"
import "qrc:/Util/Functions.js" as Functions

Item
{
	id: comboBoxID

	property var viewSourceController
	property string value: viewSourceController.value

	width: textID.width + imageID.width + Functions.GetMargin()

	CustomText
	{
		id: textID
		text: qsTranslate("ViewSource", viewSourceController.title)
	}

	Image
	{
		id: imageID
		height: parent.height * 0.5
		fillMode: Image.PreserveAspectFit
		anchors
		{
			left: textID.right
			leftMargin: Functions.GetMargin()
			verticalCenter: parent.verticalCenter
		}
		source: "qrc:/icons/expander.png"
		rotation: popupID.popuped ? 0 : 90
	}

	DynamicMenu
	{
		id: popupID
		model: viewSourceController.GetModel()
		delegate: MenuItem
		{
			text: qsTranslate("ViewSource", Title)
			checked: Value === comboBoxID.value
			onTriggered: if (Value !== comboBoxID.value)
				viewSourceController.value = Value
		}
	}

	MouseArea
	{
		anchors.fill: parent
		onClicked: popupID.popup()
	}
}
