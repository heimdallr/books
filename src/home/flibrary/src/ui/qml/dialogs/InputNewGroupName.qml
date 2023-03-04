import QtQuick 2.15

InputStringDialog
{
	id: inputStringDialogID

	property var controller

	function checkNewName()
	{
		if (visible)
			controller.CheckNewName(text)
	}

	title: qsTranslate("NewGroupDialog", "Input new group name")
	inputStringTitle: qsTranslate("NewGroupDialog", "Group name")
	text: qsTranslate("NewGroupDialog", "New group")

	errorText: controller.errorText
	okEnabled: text !== "" && errorText === "" && !controller.checkNewNameInProgress

	onTextChanged: checkNewName()
	onVisibleChanged: checkNewName()
}
