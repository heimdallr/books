import QtQuick 2.15

import HomeCompa.Flibrary.ModelControllerType 1.0

import "qrc:/ViewTemplate"

ViewTemplate
{
	id: listViewID
	modelControllerType: ModelControllerType.Navigation
	loadPath: "qrc:/Navigation/"
	viewSourceComboBoxController: guiController.GetViewSourceComboBoxNavigationController()
}
