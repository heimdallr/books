import QtQuick 2.15
import QtQuick.Controls 1.4

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
