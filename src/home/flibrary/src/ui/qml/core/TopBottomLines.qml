import QtQuick 2.15

Item
{
	Rectangle
	{
		height: 1
		color: uiSettings.borderColor
		anchors
		{
			left: parent.left
			right: parent.right
			top: parent.top
		}
	}

	Rectangle
	{
		height: 1
		color: uiSettings.borderColor
		anchors
		{
			left: parent.left
			right: parent.right
			bottom: parent.bottom
			bottomMargin: -1
		}
	}
}
