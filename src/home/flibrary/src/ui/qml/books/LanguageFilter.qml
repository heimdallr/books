import QtQuick 2.15
import QtQuick.Controls 1.4

ComboBox
{
	model: guiController.languages
	onCurrentIndexChanged: guiController.language = currentText
	onModelChanged: currentIndex = find(guiController.language)
}
