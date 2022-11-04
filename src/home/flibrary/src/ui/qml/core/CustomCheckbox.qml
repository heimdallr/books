import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4

import "qrc:/Core/constants.js" as Constants

CheckBox
{
	id: checkBoxID

	style: CheckBoxStyle
	{
		indicator: Item
		{
			height: Constants.checkboxSize
			width: height
			Image
			{
				function iconName()
				{
					switch(checkedState)
					{
						case Qt.Checked: return "checked"
						case Qt.Unchecked: return "unchecked"
						case Qt.PartiallyChecked : return "partially"
					}
				}

				anchors.fill: parent
				fillMode: Image.PreserveAspectFit
				source: ("qrc:/icons/checkbox/%1.png").arg(iconName())
			}
		}
	}
}
