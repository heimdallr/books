import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle
{
	id: navigationID

	ListView
	{
		id: listViewID

		anchors.fill: parent

		model: guiController.GetAuthorsModel()

		focus: true

		Keys.onUpPressed: if (listViewID.currentIndex > 0)
			listViewID.currentIndex--
		Keys.onDownPressed: if (listViewID.currentIndex < listViewID.count - 1)
			listViewID.currentIndex++

		flickableDirection: Flickable.VerticalFlick
		boundsBehavior: Flickable.StopAtBounds
		ScrollBar.vertical: ScrollBar {}

		onCurrentIndexChanged: guiController.OnCurrentAuthorChanged(currentIndex)

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

			MouseArea {
				anchors.fill: parent
				onClicked:
				{
					listViewID.focus = true
					listViewID.currentIndex = index
				}
			}
		}
	}
}
