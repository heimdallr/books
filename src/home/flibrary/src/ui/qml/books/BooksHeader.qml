import QtQuick 2.15
import QtQuick.Controls 2.15

import "qrc:/Core"

SplitView
{
	property alias authorsVisible: authorID.visible
	property alias seriesVisible: seriesID.visible
	property alias genresVisible: genreID.visible

	id: headerID

	orientation: Qt.Horizontal
	handle: SplitViewHandle {}

	CustomText
	{
		id: authorID
		text: qsTranslate("Header", "Author")
	}

	CustomText
	{
		id: seriesID
		text: qsTranslate("Header", "SeriesTitle")
	}

	CustomText
	{
		SplitView.fillWidth: true
		text: qsTranslate("Header", "Title")
	}

	CustomText
	{
		id: genreID
		text: qsTranslate("Header", "GenreAlias")
	}

	LanguageFilter
	{
		width: 36
		SplitView.preferredWidth: 50
	}
}
