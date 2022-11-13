import QtQuick 2.15
import QtQuick.Controls 2.15

import "qrc:/Core"

SplitView
{
	id: viewID

	property alias authorsVisible: authorID.visible
	property alias seriesVisible: seriesID.visible
	property alias genresVisible: genreID.visible

	orientation: Qt.Horizontal
	handle: SplitViewHandle {}

	CustomText
	{
		id: authorID
		text: qsTranslate("Header", "Author")
		SplitView.preferredWidth: SplitView.fillWidth ? 0 : viewID.width * uiSettings.widthAuthor
		SplitView.fillWidth: visible
		onWidthChanged:
		{
//			console.log(`head author width: ${width}`)
			if (viewID.width > 1) uiSettings.widthAuthor = width / viewID.width
		}
	}

	CustomText
	{
		SplitView.preferredWidth: viewID.width * uiSettings.widthTitle
		SplitView.fillWidth: !authorID.visible
		onWidthChanged:
		{
//			console.log(`book title width: ${width}`)
			if (viewID.width > 1) uiSettings.widthTitle = width / viewID.width
		}
		text: qsTranslate("Header", "Title")
	}

	CustomText
	{
		id: seriesID
		text: qsTranslate("Header", "SeriesTitle")
		SplitView.preferredWidth: viewID.width * uiSettings.widthSeries
		onWidthChanged:
		{
//			console.log(`head series width: ${width}`)
			if (viewID.width > 1) uiSettings.widthSeries = width / viewID.width
		}
	}

	CustomText
	{
		text: qsTranslate("Header", "No")
		SplitView.preferredWidth: uiSettings.widthSeqNo
		onWidthChanged:
		{
//			console.log(`head seqNo width: ${width}`)
			uiSettings.widthSeqNo = width
		}
	}

	CustomText
	{
		id: genreID
		SplitView.preferredWidth: viewID.width * uiSettings.widthGenre
		onWidthChanged:
		{
//			console.log(`head genre width: ${width}`)
			if (viewID.width > 1) uiSettings.widthGenre = width / viewID.width
		}
		text: qsTranslate("Header", "GenreAlias")
	}

	LanguageFilter
	{
		SplitView.preferredWidth: uiSettings.widthLanguage
		onWidthChanged:
		{
//			console.log(`head language width: ${width}`)
			uiSettings.widthLanguage = width
		}
	}
}
