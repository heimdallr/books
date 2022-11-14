import QtQuick 2.15
import QtQuick.Controls 1.2

Rectangle
{
	id: viewModeComboBoxID

	property string value

	height: uiSettings.heightRow
	width: height

	ExclusiveGroup { id: menuExclusiveGroup }

	Action
	{
		id: actionFindID
		text: qsTranslate("ViewMode", "Find")
		iconSource: "icons/find.png"
		checkable: true
		exclusiveGroup: menuExclusiveGroup
		onTriggered:
		{
			viewModeComboBoxID.value = "Find"
			imageID.source = iconSource
		}
	}

	Action
	{
		id: actionFilterID
		text: qsTranslate("ViewMode", "Filter")
		iconSource: "icons/filter.png"
		checkable: true
		exclusiveGroup: menuExclusiveGroup
		onTriggered:
		{
			viewModeComboBoxID.value = "Filter"
			imageID.source = iconSource
		}
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
		fillMode: Image.PreserveAspectFit
	}

	MouseArea
	{
		anchors.fill: parent
		onClicked: menuID.popup()
	}

	Component.onCompleted:
	{
		actionFindID.checked = true
		actionFindID.triggered()
	}
}
