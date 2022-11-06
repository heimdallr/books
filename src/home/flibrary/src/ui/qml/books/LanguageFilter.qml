import QtQuick 2.15
import QtQuick.Controls 1.4

ComboBox
{
	model: localeController.languages
	onCurrentIndexChanged: localeController.language = currentText
	onModelChanged: currentIndex = find(localeController.language)
}
