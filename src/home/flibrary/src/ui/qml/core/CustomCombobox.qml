import QtQuick 2.15
import QtQuick.Controls 2.15

ComboBox
{
	id: comboBoxID

	function add(text, value)
	{
		viewModeModelID.append({"text": text, "value": value})

	}

	model: viewModeModelID
	textRole: "text"
	valueRole: "value"

	ListModel { id: viewModeModelID }
}
