import QtQuick 2.15
import QtQuick.Controls 1.4

import "qrc:/Core"
import "qrc:/Util/Functions.js" as Functions

Item
{
	id: comboBoxID

	property var comboBoxController
	readonly property string comboBoxValue: comboBoxController.value

	readonly property int preferredWidth: textID.width + imageID.width + 3 * Functions.GetMargin()

	Row
	{
		height: parent.height
		anchors.top: parent.top
		spacing: Functions.GetMargin()
		padding: spacing

		CustomText
		{
			id: textID
			anchors
			{
				bottom: parent.bottom
				bottomMargin: Functions.GetMargin()
			}
			text: qsTranslate("ViewSource", comboBoxController.title)
		}

		Image
		{
			id: imageID
			height: parent.height * 0.5
			width: height
			fillMode: Image.PreserveAspectFit
			anchors
			{
				bottom: parent.bottom
				bottomMargin: uiSettings.sizeFont / 2
			}
			source: "qrc:/icons/expander.png"
			rotation: popupID.popuped ? 0 : 90
		}
	}

	DynamicMenu
	{
		id: popupID
		model: comboBoxController.GetModel()
		delegate: MenuItem
		{
			text: qsTranslate("ViewSource", Title)
			checked: Value === comboBoxID.comboBoxValue
			onTriggered: if (Value !== comboBoxID.comboBoxValue)
				comboBoxController.value = Value
		}
	}

	MouseArea
	{
		anchors.fill: parent
		onClicked: popupID.popup()
	}
}
