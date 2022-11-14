import QtQuick 2.15
import QtQuick.Controls 1.4

import "qrc:/Core"

ListView
{
	id: logViewID

	model: logController.GetModel()

	flickableDirection: Flickable.VerticalFlick
//	ScrollBar.vertical: ScrollBar {}
	delegate: CustomText
	{
		height: uiSettings.heightRow
		text: display
	}
}
