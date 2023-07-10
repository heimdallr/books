import QtQuick 2.15

Item
{
	id: expanderID
	property bool expanded

	signal clicked()

	Image
	{
		id: imageID
		anchors.fill: parent
		fillMode: Image.PreserveAspectFit
		source: "qrc:/icons/expander.png"
		transformOrigin: Item.Center
		rotation: expanded ? 90 : 0
	}

	MouseArea
	{
		anchors. fill: parent
		onClicked: expanderID.clicked()
	}
}
