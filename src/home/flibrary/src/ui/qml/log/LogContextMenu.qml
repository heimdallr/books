import QtQuick 2.15
import QtQuick.Controls 1.4

Menu
{
	title: qsTranslate("Logging", "Log")

	MenuItem
	{
		text: log.logMode ? qsTranslate("Logging", "Hide log") : qsTranslate("Logging", "Show log")
		onTriggered: log.logMode = ! log.logMode
	}

	MenuItem
	{
		text: qsTranslate("Logging", "Clear log")
		onTriggered: log.Clear()
	}

	MenuItem
	{
		text: qsTranslate("Logging", "Show collection statistics")
		onTriggered:
		{
			log.logMode = true
			guiController.LogCollectionStatistics()
		}
	}

	SeverityLevel{}
}
