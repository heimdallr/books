import QtQuick 2.15
import QtQuick.Controls 1.4

Menu
{
	title: qsTranslate("Logging", "Log")

	MenuItem
	{
		text: logController.logMode ? qsTranslate("Logging", "Hide log") : qsTranslate("Logging", "Show log")
		onTriggered: logController.logMode = ! logController.logMode
	}

	SeverityLevel{}
}
