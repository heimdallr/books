import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle
{
	id: handleDelegate

	implicitWidth: uiSettings.splitViewHandleSize
	implicitHeight: uiSettings.splitViewHandleSize
	color
		: SplitHandle.pressed ?	uiSettings.splitViewHandlePressedColor
		: SplitHandle.hovered ?	Qt.lighter(uiSettings.splitViewHandleColor, 1.2)
		:						uiSettings.splitViewHandleColor

/*	containmentMask: Item
	{
		x: (handleDelegate.width - width) / 2
		y: undefined
		width: 64
		height: splitViewID.height
	}*/
}
