import QtQuick 2.12
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.1
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

import Style 1.0

import "qrc:/Core"
import "qrc:/Util/Functions.js" as Functions

Window
{
	id: dialogID

	property alias inputStringTitle: inputStringTitleID.text
	property alias text: inputStringTextID.text
	property alias errorText: errorTextID.text
	property alias okEnabled: btnOkID.enabled

	signal accepted()
	signal rejected()

	transientParent: applicationWindowID

	width: uiSettings.heightRow * 30
	minimumHeight: guiController.GetPixelMetric(Style.PM_TitleBarHeight)
		+ inputStringLayoutID.height
		+ buttonsLayoutID.height
		+ 18 * Functions.GetMargin()
	maximumHeight: minimumHeight

	flags: Qt.Dialog

	onVisibleChanged: if (visible)
	{
		inputStringTextID.focus = true
		inputStringTextID.selectAll()
	}

	ColumnLayout
	{
		id: columnLayoutID

		anchors.fill: parent

		RowLayout
		{
			id: inputStringLayoutID

			Layout.margins: Functions.GetMargin() * 2
			CustomText
			{
				id: inputStringTitleID
				Layout.preferredWidth: dialogID.width * uiSettings.widthAddCollectionDialogText
			}

			TextField
			{
				id: inputStringTextID
				Layout.fillWidth: true
				font.pointSize: uiSettings.sizeFont
			}
		}


		RowLayout
		{
			id: buttonsLayoutID

			Layout.margins: Functions.GetMargin() * 2
			Layout.fillWidth: true

			CustomText
			{
				id: errorTextID
				Layout.preferredHeight: uiSettings.heightRow
				Layout.fillWidth: true

				horizontalAlignment: Text.AlignRight
				font.pointSize: uiSettings.sizeFont * uiSettings.sizeFontError
				color: uiSettings.colorErrorText
			}

			Button
			{
				id: btnOkID
				Layout.preferredHeight: uiSettings.heightRow
			    text: qsTranslate("QPlatformTheme", "OK")
				enabled: dialogID.okEnabled
				onClicked:
				{
					accepted()
					dialogID.visible = false
				}
			}

			Button
			{
				id: btnCancelID
				Layout.preferredHeight: uiSettings.heightRow
			    text: qsTranslate("QPlatformTheme", "Cancel")
				onClicked:
				{
					rejected()
					dialogID.visible = false
				}
			}
		}
	}
}
