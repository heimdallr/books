import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import Style 1.0

import "qrc:/Util/Functions.js" as Functions

Item
{
	id: customListViewID

	property alias delegate: listViewID.delegate

	ColumnLayout
	{
		anchors.fill: parent
		spacing: Functions.GetMargin()

		Component
		{
			id: highlightID
			Rectangle
			{
				width: customListViewID.width
				height: uiSettings.heightRow

				color: modelController.focused ? uiSettings.colorHighlight : uiSettings.colorHighlightUnfocused
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
			ScrollBar.vertical: ScrollBar { width: guiController.GetPixelMetric(Style.PM_ScrollBarExtent) * uiSettings.sizeScrollbar }

			highlight: highlightID
			highlightFollowsCurrentItem: false

			onHeightChanged: modelController.SetPageSize(height / uiSettings.heightRow)
		}
	}
}