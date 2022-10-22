import QtQuick 2.15
import QtQuick.Controls 2.15
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
//		modelController.currentIndex = -1
		loaderID.setSource(loadPath + viewSourceComboBoxID.currentValue + ".qml")
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
				viewTemplateID.modelController.SetViewMode(viewModeComboBoxID.currentValue, viewModeTextID.text)
			}

			CustomCombobox
			{
				id: viewSourceComboBoxID
				currentIndex: -1
				onCurrentValueChanged: onSourceChanged()
			}

			TextField
			{
				id: viewModeTextID
				Layout.fillWidth: true
				font.pointSize: Constants.fontSize
				onTextChanged: findLayoutID.setViewMode()
			}

			CustomCombobox
			{
				id: viewModeComboBoxID
				onActivated: findLayoutID.setViewMode()
				Component.onCompleted:
				{
					add(qsTranslate("ViewMode", "Find"), "Find")
					add(qsTranslate("ViewMode", "Filter"), "Filter")
					currentIndex = indexOfValue("Find")
				}
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
