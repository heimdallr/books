import QtQuick 2.15
import QtQuick.Controls 2.15

import "qrc:/Core"

ListView
{
	id: logViewID

	model: logController.GetModel()

	flickableDirection: Flickable.VerticalFlick
	ScrollBar.vertical: ScrollBar { id: scrollBarID }
	delegate: CustomText
	{
		height: uiSettings.heightRow
		text: display
	}

//	onCountChanged:  scrollBarID.position = 1.0 - scrollBarID.size
}
