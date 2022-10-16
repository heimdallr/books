import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle
{
	id: navigationID

	readonly property var modelController: guiController.GetAuthorsModelController()

	RowLayout
	{
		id: findLayoutID
		spacing: 4

		anchors
		{
			top: parent.top
			left: parent.left
			right: parent.right
		}

		function setViewMode()
		{
			navigationID.modelController.SetViewMode(viewModeComboBoxID.currentValue, viewModeTextID.text)
		}

		ComboBox
		{
			id: viewModeComboBoxID
			ListModel
			{
				id: viewModeModelID
		    }

			model: viewModeModelID
			textRole: "text"
			valueRole: "value"

			onActivated: findLayoutID.setViewMode()

			Component.onCompleted:
			{
				viewModeModelID.append({"text": qsTranslate("ViewMode", "Find"), "value": "Find"})
				viewModeModelID.append({"text": qsTranslate("ViewMode", "Filter"), "value": "Filter"})
				currentIndex = indexOfValue("Find")
			}
		}

		TextField
		{
			id: viewModeTextID
			Layout.fillWidth: true
			font.pointSize: 12
			onTextChanged: findLayoutID.setViewMode()
		}
	}

	ListView
	{
		id: listViewID

		anchors
		{
			top: findLayoutID.bottom
			bottom: parent.bottom
			left: parent.left
			right: parent.right
		}

		model: modelController.model
		currentIndex: modelController.currentIndex

		clip: true
		boundsBehavior: Flickable.DragAndOvershootBounds
		snapMode: ListView.SnapToItem

		flickableDirection: Flickable.VerticalFlick
		ScrollBar.vertical: ScrollBar { id: scrollBarID }

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
				onClicked: modelController.currentIndex = index
			}
		}
	}
}
