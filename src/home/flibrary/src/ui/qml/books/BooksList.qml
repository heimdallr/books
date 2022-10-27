import QtQuick 2.15

import "qrc:/"

ViewTemplate
{
	id: listViewID
	loadPath: "Book/"
	Component.onCompleted:
	{
		viewSourceComboBox.add(qsTranslate("ViewSource", "List"), "BooksListView")
		viewSourceComboBox.add(qsTranslate("ViewSource", "Tree"), "BooksTreeView")
		viewSourceComboBox.currentIndex = 0
	}
}
