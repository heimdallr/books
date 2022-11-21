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

	SeverityLevel{}
}
