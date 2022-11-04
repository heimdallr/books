import QtQuick 2.15

Item
{
	id: expanderID
	property bool expanded

	signal clicked()

	Image
	{
		anchors.fill: parent
		fillMode: Image.PreserveAspectFit
		source: ("qrc:/icons/expander/%1.png").arg(expanded ? "minus" : "plus")
	}

	MouseArea
	{
		anchors. fill: parent
		onClicked: expanderID.clicked()
	}
}
