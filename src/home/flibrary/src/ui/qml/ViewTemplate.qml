import QtQuick 2.15
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.15

import "Core"
import "qrc:/Util/Functions.js" as Functions

Rectangle
{
	id: viewTemplateID

	property var modelController
	property string loadPath
	property alias viewSourceComboBox: viewSourceComboBoxID

	function onSourceChanged()
	{
		viewModeTextID.text = ""
		loaderID.setSource(loadPath + viewSourceComboBox.model.get(viewSourceComboBox.currentIndex).value + ".qml")
		applicationWindowID.focus = true
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

			function setViewMode()
			{
				modelController.SetViewMode(viewModeComboBoxID.value, viewModeTextID.text)
			}

			CustomCombobox
			{
				Layout.leftMargin: Functions.GetMargin()
				Layout.preferredWidth: uiSettings.heightRow * 5 / 2
				Layout.preferredHeight: uiSettings.heightRow
				id: viewSourceComboBoxID
				currentIndex: -1
				onCurrentIndexChanged: onSourceChanged()
			}

			TextField
			{
				id: viewModeTextID
				Layout.fillWidth: true
				font.pointSize: uiSettings.sizeFont
				onTextChanged: findLayoutID.setViewMode()
			}

			ViewModeCombobox
			{
				id: viewModeComboBoxID
				onValueChanged: findLayoutID.setViewMode()
			}

			CustomText
			{
				Layout.rightMargin: Functions.GetMargin()
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
