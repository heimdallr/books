import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle
{
	id: handleDelegate

	implicitWidth: uiSettings.sizeSplitViewHandle
	implicitHeight: uiSettings.sizeSplitViewHandle
	color
		: SplitHandle.pressed ?	uiSettings.colorSplitViewHandlePressed
		: SplitHandle.hovered ?	Qt.lighter(uiSettings.colorSplitViewHandle, 1.2)
		:						uiSettings.colorSplitViewHandle

/*	containmentMask: Item
	{
		x: (handleDelegate.width - width) / 2
		y: undefined
		width: 64
		height: splitViewID.height
	}*/
}
