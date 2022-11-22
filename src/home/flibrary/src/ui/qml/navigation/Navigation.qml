import QtQuick 2.15

import "qrc:/ViewTemplate"

ViewTemplate
{
	id: listViewID
	loadPath: "qrc:/Navigation/"
	viewSourceController: guiController.GetViewSourceNavigationController()
}
