import QtQuick 2.15

import "qrc:/"

ViewTemplate
{
	id: listViewID
	loadPath: "Navigation/"

	Component.onCompleted:
	{
		viewSourceComboBox.add(qsTranslate("ViewSource", "Authors"), "Authors")
		viewSourceComboBox.add(qsTranslate("ViewSource", "Series"), "Series")
		viewSourceComboBox.add(qsTranslate("ViewSource", "Genres"), "Genres")
		viewSourceComboBox.currentIndex = 0
	}
}
