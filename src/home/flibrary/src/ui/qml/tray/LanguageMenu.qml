import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.1

Menu
{
	id: localeMenuID
	title: qsTranslate("Tray", "Language")

	readonly property string currentLanguage: localeController.locale

	ExclusiveGroup
	{
		id: languageExclusiveGroupID
	}

	MessageDialog
	{
	    id: restartConfirmDialogID
	    title: qsTranslate("Common", "Warning")
	    text: qsTranslate("Collection", "You must restart the application to apply the changes.\nRestart now?")
		standardButtons: StandardButton.Yes | StandardButton.No
	    onYes: Qt.exit(1234)
	}

	Instantiator
	{
		model: localeController.locales
		MenuItem
		{
			text: qsTranslate("Language", modelData)
			checkable: true
			checked: modelData === localeMenuID.currentLanguage
			exclusiveGroup: languageExclusiveGroupID
			onTriggered:
			{
				if (modelData === localeMenuID.currentLanguage)
					return;

				localeController.locale = modelData
				restartConfirmDialogID.open()
			}
		}
		onObjectAdded: localeMenuID.insertItem(index, object)
		onObjectRemoved: localeMenuID.removeItem(object)
	}
}
