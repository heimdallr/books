import QtQuick 2.15
import QtQuick.Controls 1.4

Menu
{
	title: qsTranslate("Tray", "Settings")

	LanguageMenu {}

	MenuItem
	{
		text: qsTranslate("Tray", "Restore default values")
		onTriggered: uiSettings.Reset()
	}
}
