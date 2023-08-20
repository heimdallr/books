import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Window 2.0

Item
{
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