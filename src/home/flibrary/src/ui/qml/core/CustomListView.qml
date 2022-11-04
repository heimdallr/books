import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import "constants.js" as Constants

Item
{
	id: customListViewID

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
		}
	}
}
