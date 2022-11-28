import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.15

import "qrc:/Core"
import "qrc:/Util/Functions.js" as Functions

Rectangle
{
	id: viewTemplateID

	property var modelController
	property string loadPath
	property alias viewSourceComboBoxController: viewSourceComboBoxID.comboBoxController
	property alias viewSourceComboBoxValue: viewSourceComboBoxID.comboBoxValue

	onViewSourceComboBoxValueChanged:
	{
		viewModeTextID.text = ""
		loaderID.setSource(loadPath + viewSourceComboBoxValue + ".qml")
	}

	ColumnLayout
	{
		anchors.fill: parent
		spacing: Functions.GetMargin()

		RowLayout
		{
			id: findLayoutID
			spacing: Functions.GetMargin()
			Layout.fillWidth: true

			CustomComboBox
			{
				id: viewSourceComboBoxID
				translationContext: "ViewSource"
				Layout.preferredHeight: uiSettings.heightRow
				Layout.preferredWidth: preferredWidth
			}

			TextField
			{
				id: viewModeTextID
				Layout.fillWidth: true
				font.pointSize: uiSettings.sizeFont
				onTextChanged: modelController.viewModeValue = text
				Component.onCompleted: text = modelController.viewModeValue
			}

			ViewModeCombobox
			{
				id: viewModeComboBoxID
				controller: modelController
			}

			CustomText
			{
				Layout.rightMargin: Functions.GetMargin()
				Layout.preferredWidth: preferredWidth + Functions.GetMargin()
				text: modelController ? modelController.count : ""
			}
		}

		Loader
		{
			id: loaderID
			Layout.fillWidth: true
			Layout.fillHeight: true
		}
	}
}
