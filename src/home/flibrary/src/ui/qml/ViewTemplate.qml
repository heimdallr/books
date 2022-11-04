import QtQuick 2.15
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.15

import "Core"
import "../Core/constants.js" as Constants

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
		spacing: 4

		RowLayout
		{
			id: findLayoutID
			spacing: 4
			Layout.fillWidth: true

			function setViewMode()
			{
				modelController.SetViewMode(viewModeComboBoxID.value, viewModeTextID.text)
			}

			CustomCombobox
			{
				Layout.leftMargin: 4
				id: viewSourceComboBoxID
				currentIndex: -1
				onCurrentIndexChanged: onSourceChanged()
			}

			TextField
			{
				id: viewModeTextID
				Layout.fillWidth: true
				font.pointSize: Constants.fontSize
				onTextChanged: findLayoutID.setViewMode()
			}

			ViewModeCombobox
			{
				id: viewModeComboBoxID
				onValueChanged: findLayoutID.setViewMode()
			}

			CustomText
			{
				Layout.rightMargin: 4
				text: modelController.count
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
