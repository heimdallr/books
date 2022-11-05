import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.1
import QtQuick.Window 2.0
import QSystemTrayIcon 1.0

import "Dialogs"

Item
{
	QSystemTrayIcon
	{
		id: systemTray

		Component.onCompleted:
		{
			icon = iconTray
			toolTip = qsTranslate("Tray", "Flibrary")
			show()
		}

		onActivated:
		{
			if (reason === 1)
				trayMenu.popup()
			else
				applicationWindowID.visibility === Window.Hidden
					? applicationWindowID.show()
					: applicationWindowID.hide()
		}
	}

	MessageDialog
	{
	    id: removeConfirmDialogID
	    title: qsTranslate("Common", "Warning")
	    text: qsTranslate("Collection", "Are you sure you want to delete the collection?")
		standardButtons: StandardButton.Yes | StandardButton.No
	    onYes:
		{
			console.log(`accepted`)
	    }
	}

	AddCollection
	{
		id: addCollectionID
	}

	Menu
	{
        id: trayMenu

		Menu
		{
			title: qsTranslate("Tray", "Collections")

	        MenuItem
			{
	            text: qsTranslate("Tray", "Add new collection...")
				onTriggered: addCollectionID.show()
	        }

	        Menu
			{
	            title: qsTranslate("Tray", "Select collection")
				enabled: false
	        }

	        MenuItem
			{
	            text: qsTranslate("Tray", "Remove collection")
				onTriggered: removeConfirmDialogID.open()
	        }
		}

		Menu
		{
			title: qsTranslate("Tray", "View")

	        MenuItem
			{
	            text: uiSettings.showDeleted == 0 ? qsTranslate("Tray", "Show deleted books") : qsTranslate("Tray", "Hide deleted books")
	            onTriggered: uiSettings.showDeleted = uiSettings.showDeleted == 0 ? 1 : 0
	        }

	        MenuItem
			{
	            text: uiSettings.showBookInfo == 0 ? qsTranslate("Tray", "Show annotation") : qsTranslate("Tray", "Hide annotation")
	            onTriggered: uiSettings.showBookInfo = uiSettings.showBookInfo == 0 ? 1 : 0
	        }
		}

		Menu
		{
			title: qsTranslate("Tray", "Settings")

			Menu
			{
				id: localeMenuID
				title: qsTranslate("Tray", "Language")

				readonly property string currentLanguage: guiController.locale

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
				    onYes: guiController.Restart()
				}

				Instantiator
				{
					model: guiController.locales
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

							guiController.locale = modelData
							restartConfirmDialogID.open()
						}
					}
					onObjectAdded: localeMenuID.insertItem(index, object)
					onObjectRemoved: localeMenuID.removeItem(object)
				}
			}
		}

        MenuItem
		{
            text: applicationWindowID.visibility === Window.Hidden ? qsTranslate("Tray", "Show Flibrary") : qsTranslate("Tray", "Hide Flibrary")
            onTriggered: applicationWindowID.visibility === Window.Hidden ? applicationWindowID.show() : applicationWindowID.hide()
        }

        MenuItem
		{
            text: qsTranslate("Tray", "Exit")
            onTriggered: Qt.quit()
        }
    }
}
