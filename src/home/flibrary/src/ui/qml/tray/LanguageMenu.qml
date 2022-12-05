import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.1

import "qrc:/Core"

DynamicMenu
{
	id: localeMenuID
	title: qsTranslate("Tray", "Language")

	readonly property string currentLanguage: localeController.locale

	MessageDialog
	{
	    id: restartConfirmDialogID
	    title: qsTranslate("Common", "Warning")
	    text: qsTranslate("Collection", "You must restart the application to apply the changes.\nRestart now?")
		standardButtons: StandardButton.Yes | StandardButton.No
		icon: StandardIcon.Question
	    onYes: Qt.exit(1234)
	}

	model: localeController.locales
	delegate: MenuItem
	{
		text: qsTranslate("Language", modelData)
		checked: modelData === localeMenuID.currentLanguage
		exclusiveGroup: languageExclusiveGroupID
		onTriggered: if (modelData !== localeMenuID.currentLanguage)
		{
			localeController.locale = modelData
			restartConfirmDialogID.open()
		}
	}
}
