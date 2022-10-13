import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle
{
	id: handleDelegate

	implicitWidth: 4
	implicitHeight: 4
	color
		: SplitHandle.pressed ?	"#81e889"
		: SplitHandle.hovered ?	Qt.lighter("#c2f4c6", 1.2)
		:						"#c2f4c6"

/*	containmentMask: Item
	{
		x: (handleDelegate.width - width) / 2
		y: undefined
		width: 64
		height: splitViewID.height
	}*/
}
