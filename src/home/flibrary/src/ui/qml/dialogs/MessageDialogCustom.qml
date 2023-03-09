import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.1

MessageDialog
{
	property var controller

	visible: controller.visible
	onClickedButtonChanged: controller.OnButtonClicked(clickedButton)
	onRejected: controller.visible = false
}
