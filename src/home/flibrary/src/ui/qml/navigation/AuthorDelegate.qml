import QtQuick 2.15

Rectangle
{
	id: autorDelegateID

	height: 36
	color: "transparent"

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

		color: "black"
		text: Title
	}

	MouseArea
	{
		anchors.fill: parent
		onClicked: Click = true
	}
}
