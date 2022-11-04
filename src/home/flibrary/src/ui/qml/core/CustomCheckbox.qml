import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4

import "qrc:/Core/constants.js" as Constants

Item
{
	id: checkBoxID

	property int checkedState: 0
	signal clicked()

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

	MouseArea
	{
		anchors.fill: parent
		onClicked: checkBoxID.clicked()
	}
}
