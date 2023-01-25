import QtQuick 2.15
import QtQuick.Controls 1.4

Menu
{
	id: dynamicMenuID

	property alias model: instantiatorID.model
	property alias delegate: instantiatorID.delegate
	property bool popuped: false
	property bool checkable: true
	property bool alwaysEnabled: false

	enabled: false

	onAboutToShow: popuped = true
	onAboutToHide: popuped = false

	ExclusiveGroup
	{
		id: exclusiveGroupID
	}

	Instantiator
	{
		id: instantiatorID
		onObjectAdded:
		{
			dynamicMenuID.insertItem(index, object)
			dynamicMenuID.enabled = true
			object.checkable = dynamicMenuID.checkable
			if (dynamicMenuID.checkable)
				object.exclusiveGroup = exclusiveGroupID
		}

		onObjectRemoved:
		{
			dynamicMenuID.removeItem(object)
			if (!dynamicMenuID.items.count && !alwaysEnabled)
				dynamicMenuID.enabled = false
		}
	}
}
