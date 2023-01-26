import QtQuick 2.15

InputStringDialog
{
	id: inputStringDialogID

	property var controller

	title: qsTranslate("NewGroupDialog", "Input new group name")
	inputStringTitle: qsTranslate("NewGroupDialog", "Group name")
	text: qsTranslate("NewGroupDialog", "New group")

	errorText: controller.errorText
	okEnabled: text !== "" && errorText === "" && !controller.checkNewNameInProgress

	onTextChanged: controller.CheckNewName(text)
}
