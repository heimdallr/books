import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle
{
	id: navigationID

	property var modelController: undefined
	property alias delegate: listViewID.delegate

	ColumnLayout
	{
		anchors.fill: parent
		spacing: 4

		RowLayout
		{
			id: findLayoutID
			spacing: 4
			Layout.fillWidth: true

			function setViewMode()
			{
				navigationID.modelController.SetViewMode(viewModeComboBoxID.currentValue, viewModeTextID.text)
			}

			ComboBox
			{
				id: viewModeComboBoxID

				model: viewModeModelID
				textRole: "text"
				valueRole: "value"

				onActivated: findLayoutID.setViewMode()

				ListModel { id: viewModeModelID }

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

		Component
		{
		    id: highlightID
		    Rectangle
			{
		        width: navigationID.width; height: 36; radius: 5
		        color: "lightsteelblue"
		        y: listViewID.currentItem.y
		        Behavior on y
				{
		            SpringAnimation
					{
		                spring: 3
		                damping: 0.3
		            }
		        }
		    }
		}

		ListView
		{
			id: listViewID

			Layout.fillWidth: true
			Layout.fillHeight: true

			model: modelController.model
			currentIndex: modelController.currentIndex

			clip: true
			boundsBehavior: Flickable.DragAndOvershootBounds
			snapMode: ListView.SnapToItem

			flickableDirection: Flickable.VerticalFlick
			ScrollBar.vertical: ScrollBar {}

			highlight: highlightID
			highlightFollowsCurrentItem: false

			onHeightChanged: modelController.SetPageSize(height / currentItem.height)
		}

		Rectangle
		{
			height: 36
			Layout.fillWidth: true
			border { color: "lightgray"; width: 1 }

			Text
			{
				anchors
				{
					right: parent.right
					rightMargin: 4
					bottom: parent.bottom
					bottomMargin: 4
				}
				font.pointSize: 10
				text: qsTranslate("Navigation", "Total: %1").arg(listViewID.count)
			}
		}
	}
}
