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
			Layout.fillWidth: true
			Layout.fillHeight: true
			source: loadPath + viewSourceComboBoxID.currentValue + ".qml"
		}
	}
}
