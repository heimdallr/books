import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle
{
	id: autorDelegateID

	property var onClickedFunction: undefined

	property alias backgroundColor: autorDelegateID.color
	property alias textColor: textID.color
	property alias text: textID.text

	height: 36

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
	}

	MouseArea
	{
		anchors.fill: parent
		onClicked: onClickedFunction()
	}
}
