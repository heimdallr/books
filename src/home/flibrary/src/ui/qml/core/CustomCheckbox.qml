import QtQuick 2.15
import QtQuick.Controls 2.15

import "qrc:/Core/constants.js" as Constants

CheckBox
{
	id: checkBoxID

	indicator.width: Constants.checkboxSize
	indicator.height: Constants.checkboxSize
}
