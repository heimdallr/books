import QtQuick 2.15
import QtQuick.Controls 1.4

ComboBox
{
	model: languageController.languages
	onCurrentIndexChanged: languageController.language = currentText
	onModelChanged: currentIndex = find(languageController.language)
}
