import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle
{
	id: autorDelegateID

	property bool isSelected: false

	height: 36
	color: isSelected ? "blue" : "white"

	border { color: "lightgray"; width: 1 }

	Text
	{
		id: textID
		anchors
		{
			left: parent.left
			leftMargin: 4
			bottom: parent.bottom
			bottomMargin: 4
		}
		font.pointSize: 12

		color: isSelected ? "white" : "black"
		text: Title
	}

	MouseArea
	{
		anchors.fill: parent
		onClicked: Click = true
	}
}
