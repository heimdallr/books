import QtQuick 2.15
import QtQuick.Controls 1.4

import "qrc:/Core"

DynamicMenu
{
	id: severityLevelsMenuID

	readonly property string currentId: uiSettings.logLevel

	title: qsTranslate("Logging", "Log level")

	model: logController.GetSeverityModel()
	delegate: MenuItem
	{
		text: qsTranslate("Logging", Title)
		checked: Value === severityLevelsMenuID.currentId
		onTriggered: if (Value !== severityLevelsMenuID.currentId)
			uiSettings.logLevel = Value
	}
}
