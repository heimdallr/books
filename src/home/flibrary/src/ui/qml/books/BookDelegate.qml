import QtQuick 2.15
import QtQuick.Layouts 1.15

import "qrc:/Core/constants.js" as Constants

Rectangle
{
	id: delegateID
	property bool authorVisible: true

	height: Constants.delegateHeight
	color: "transparent"

	border { color: Constants.borderColor; width: 1 }

	RowLayout
	{
		Text
		{
			Layout.preferredWidth: delegateID.width / 4
			font.pointSize: Constants.fontSize
			text: Author
			visible: delegateID.authorVisible
		}
		Text
		{
			Layout.fillWidth: true
			font.pointSize: Constants.fontSize
			text: Title
		}
	}

	MouseArea
	{
		anchors.fill: parent
		onClicked: Click = true
	}
}
