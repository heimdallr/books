import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Window 2.0
import QSystemTrayIcon 1.0

Item
{
	QSystemTrayIcon
	{
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

	MainMenu
	{
        id: trayMenu
    }
}
