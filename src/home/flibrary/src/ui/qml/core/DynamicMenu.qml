import QtQuick 2.15
import QtQuick.Controls 1.4

Menu
{
	id: dynamicMenuID

	property alias model: instantiatorID.model
	property alias delegate: instantiatorID.delegate

	enabled: false

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
			object.exclusiveGroup = exclusiveGroupID
			object.checkable = true
		}

		onObjectRemoved:
		{
			dynamicMenuID.removeItem(object)
			if (dynamicMenuID.items.count == 0)
				dynamicMenuID.enabled = false
		}
	}
}
