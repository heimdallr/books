import QtQuick 2.15

import "qrc:/Core"

Expandable
{
	text: Title
	expanded: Expanded
	treeMargin: height * TreeLevel  * uiSettings.widthTreeMargin
	expanderVisible: ChildrenCount > 0
	onClicked: Click = true
	onExpanderClicked: Expanded = !expanded
}
