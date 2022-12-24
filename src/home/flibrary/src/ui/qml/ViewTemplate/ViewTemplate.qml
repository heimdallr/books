import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.15

import HomeCompa.Flibrary.ModelControllerType 1.0

import "qrc:/Core"
import "qrc:/Util/Functions.js" as Functions

Rectangle
{
	id: viewTemplateID

	property var modelControllerType
	property var modelController
	property string loadPath
	property alias viewSourceComboBoxController: viewSourceComboBoxID.comboBoxController
	property alias viewSourceComboBoxValue: viewSourceComboBoxID.comboBoxValue

	onViewSourceComboBoxValueChanged:
	{
		loaderID.setSource("")

		modelController
			= modelControllerType == ModelControllerType.Navigation ? guiController.GetNavigationModelController()
			: modelControllerType == ModelControllerType.Books ? guiController.GetBooksModelController()
			: undefined

		viewModeTextID.text = ""
		loaderID.setSource(loadPath + modelController.GetViewSource() + ".qml")
	}

	ColumnLayout
	{
		anchors.fill: parent
		spacing: 0

		Line
		{
			Layout.fillWidth: true
		}

		Item
		{
			Layout.fillWidth: true
			height: 2 * Functions.GetMargin()
		}

		RowLayout
		{
			id: findLayoutID
			spacing: Functions.GetMargin()
			Layout.fillWidth: true
			height: uiSettings.heightRow

			CustomComboBox
			{
				id: viewSourceComboBoxID
				translationContext: "ViewSource"
				Layout.preferredWidth: preferredWidth
				Layout.minimumHeight: findLayoutID.height
				Layout.maximumHeight: findLayoutID.height
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
				Layout.minimumWidth: findLayoutID.height
				Layout.maximumWidth: findLayoutID.height
				Layout.minimumHeight: findLayoutID.height
				Layout.maximumHeight: findLayoutID.height
				controller: modelController
			}

			CustomText
			{
				Layout.rightMargin: Functions.GetMargin()
				Layout.preferredWidth: preferredWidth + Functions.GetMargin()
				text: modelController ? modelController.count : ""
			}
		}

		Item
		{
			Layout.fillWidth: true
			height: 2 * Functions.GetMargin()
		}

		Line
		{
			Layout.fillWidth: true
		}

		Loader
		{
			id: loaderID
			Layout.fillWidth: true
			Layout.fillHeight: true
		}
	}
}
