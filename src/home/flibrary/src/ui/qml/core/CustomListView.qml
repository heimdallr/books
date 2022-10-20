import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import "constants.js" as Constants

Item
{
	id: customListViewID

	property string modelType
	property alias delegate: listViewID.delegate

	ColumnLayout
	{
		anchors.fill: parent
		spacing: 4

		Component
		{
			id: highlightID
			Rectangle
			{
				width: customListViewID.width
				height: Constants.delegateHeight
				radius: 5

				color: modelController.focused ? Constants.highlightColor : Constants.highlightColorUnfocused
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

			model: modelController.GetModel(modelType)
			currentIndex: modelController.currentIndex

			clip: true
			boundsBehavior: Flickable.DragAndOvershootBounds
			snapMode: ListView.SnapToItem

			flickableDirection: Flickable.VerticalFlick
			ScrollBar.vertical: ScrollBar {}

			highlight: highlightID
			highlightFollowsCurrentItem: false

			onHeightChanged: modelController.SetPageSize(height / Constants.delegateHeight)
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
