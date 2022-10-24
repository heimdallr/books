import QtQuick 2.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Expandable
{
	height: Constants.delegateHeight
	text: Title
	expanded: Expanded
	treeMargin: height * TreeLevel / 2
	expanderVisible: ChildrenCount > 0
	onClicked: Click = true
	onExpanderClicked: Expanded = !expanded

}
