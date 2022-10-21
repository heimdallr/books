import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Rectangle
{
	id: delegateID
	property alias authorVisible: authorID.visible

	height: Constants.delegateHeight
	color: "transparent"

	border { color: Constants.borderColor; width: 1 }

	RowLayout
	{
		id: layoutID
		anchors.fill: parent

		CheckBox
		{
			id: checkBoxID
			checked: Checked
			onClicked: Checked = !Checked
		}

		CustomText
		{
			id: authorID
			Layout.preferredWidth: layoutID.width / 8
			visible: delegateID.authorVisible
			text: Author
		}

		CustomText
		{
			Layout.fillWidth: true
			text: Title
		}

		CustomText
		{
			Layout.preferredWidth: layoutID.width / 8
			text: GenreAlias
		}

		CustomText
		{
			Layout.preferredWidth: 36
			text: Lang
		}
	}

	MouseArea
	{
		anchors.fill: parent
		anchors.leftMargin: checkBoxID.width
		onClicked: Click = true
	}
}
