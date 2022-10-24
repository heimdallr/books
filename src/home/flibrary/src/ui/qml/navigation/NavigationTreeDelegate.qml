import QtQuick 2.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Expandable
{
	height: Constants.delegateHeight
	expanded: Expanded
	level: TreeLevel
	expanderVisible: ChildrenCount > 0
	onClicked: Click = true
	onExpanderClicked: Expand = !expanded
}
