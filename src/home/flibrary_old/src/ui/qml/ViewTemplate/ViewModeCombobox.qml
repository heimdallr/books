import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle
{
	id: viewModeComboBoxID

	property var controller

	Action
	{
		id: actionFindID
		text: qsTranslate("ViewMode", "Find")
		icon.source: "qrc:/icons/find.png"
		checkable: true
		checked: controller.viewMode === "Find"
		onTriggered: controller.viewMode = "Find"
		onCheckedChanged: if (checked)
			imageID.source = icon.source
	}

	Action
	{
		id: actionFilterID
		text: qsTranslate("ViewMode", "Filter")
		icon.source: "qrc:/icons/filter.png"
		checkable: true
		checked: controller.viewMode === "Filter"
		onTriggered: controller.viewMode = "Filter"
		onCheckedChanged: if (checked)
			imageID.source = icon.source
    }

	Menu
	{
		id: menuID
		MenuItem { action: actionFindID }
		MenuItem { action: actionFilterID }
	}

	Image
	{
		id: imageID
		anchors.fill: parent
		fillMode: Image.Stretch
	}

	MouseArea
	{
		anchors.fill: parent
		onClicked: menuID.popup()
	}
}
