import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

ComboBox
{
	id: comboBoxID

	function add(text, value)
	{
		viewModeModelID.append({"text": text, "value": value})
	}

	model: viewModeModelID
	currentIndex: -1
	textRole: "text"

	ListModel { id: viewModeModelID }

//	style: ComboBoxStyle
//	{
//		background: Rectangle
//		{
//			height: control.height
//			width: control.width
//			color : "transparent"
//			border { width: 1; color: "black" }
//		}

//		label: Item
//		{
//			height: control.height
//			width: control.width
//			CustomText
//			{
//				text : control.editText
//			}
//		}
//	}
}
