import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle
{
	id: navigationID

	ListView
	{
		id: listViewID

		anchors.fill: parent

		readonly property var modelController: guiController.GetAuthorsModelController()

		model: modelController.model
		currentIndex: modelController.currentIndex

		flickableDirection: Flickable.VerticalFlick
		boundsBehavior: Flickable.StopAtBounds
		ScrollBar.vertical: ScrollBar { id: scrollBarID }

// @todo
//		onCurrentIndexChanged: scrollBarID.position = currentIndex / count

		delegate: Rectangle
		{
			readonly property bool isSelected: listViewID.currentIndex == index

			width: navigationID.width
			height: 36

			color: isSelected ?  "lightblue" : "white"

			Rectangle
			{
				anchors
				{
					top: parent.top
					left: parent.left
					right: parent.right
				}

				color: "lightgray"
				height: 1
			}

			Text
			{
				anchors
				{
					bottom: parent.bottom
					left: parent.left
		
					leftMargin: 4
					bottomMargin: 4
				}

				font.pointSize: 12
				text: display
				color: isSelected ?  "white" : "black"
			}

			MouseArea
			{
				anchors.fill: parent
				onClicked: listViewID.modelController.currentIndex = index
			}
		}
	}
}
