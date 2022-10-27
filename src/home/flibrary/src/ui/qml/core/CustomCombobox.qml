import QtQuick 2.15
import QtQuick.Controls 1.4

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
}
