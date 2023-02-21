import QtQuick 2.15
import QtQuick.Controls 2.15

import "qrc:/Book"
import "qrc:/Core"
import "qrc:/Navigation"
import "qrc:/Tray"

Item
{
	Tray {}

	SplitView
	{
		anchors.fill: parent
		orientation: Qt.Horizontal

		handle: SplitViewHandle {}

		Navigation
		{
			SplitView.minimumWidth: applicationWindowID.width / 6
			SplitView.maximumWidth: applicationWindowID.width / 2
			SplitView.preferredWidth: uiSettings.widthNavigation * applicationWindowID.width

			onWidthChanged: if (applicationWindowID.completed)
				uiSettings.widthNavigation = width / applicationWindowID.width
		}

		Books
		{
			SplitView.fillWidth: true
		}
	}

	Component.onCompleted: log.Debug(`Library view created`)
}
