import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import "constants.js" as Constants

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
				font.pointSize: Constants.fontSize
				onTextChanged: findLayoutID.setViewMode()
			}
		}

		Component
		{
			id: highlightID
			Rectangle
			{
				width: navigationID.width
				height: Constants.delegateHeight
				radius: 5

				color: listViewID.focus ? Constants.highlightColor : Constants.highlightColorUnfocused
				y: listViewID.currentItem ? listViewID.currentItem.y : 0
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

			model: modelController.GetModel()
			currentIndex: modelController.currentIndex

			clip: true
			boundsBehavior: Flickable.DragAndOvershootBounds
			snapMode: ListView.SnapToItem

			flickableDirection: Flickable.VerticalFlick
			ScrollBar.vertical: ScrollBar {}

			highlight: highlightID
			highlightFollowsCurrentItem: false

			onHeightChanged: modelController.SetPageSize(height / Constants.delegateHeight)

			Keys.onPressed: (event) => modelController.OnKeyPressed(event.key, event.modifiers)
			focus: modelController.focused
		}

		Rectangle
		{
			height: Constants.delegateHeight
			Layout.fillWidth: true
			border { color: Constants.borderColor; width: 1 }

			Text
			{
				anchors
				{
					right: parent.right
					rightMargin: 4
					bottom: parent.bottom
					bottomMargin: 4
				}
				font.pointSize: Constants.fontSizeSmall
				text: qsTranslate("Navigation", "Total: %1").arg(listViewID.count)
			}
		}
	}
}
