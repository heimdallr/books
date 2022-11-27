import QtQuick 2.15
import QtQuick.Controls 2.15

import "qrc:/Core"
import "qrc:/Util/Functions.js" as Functions

SplitView
{
	id: viewID

	orientation: Qt.Horizontal
	handle: SplitViewHandle {}

	CustomText
	{
		id: authorID
		function getIndex() { return uiSettings.indexAuthor }
		property bool ready: false
		SplitView.preferredWidth: ready ?  viewID.width * uiSettings.widthAuthor : -1
		visible: fieldsVisibilityProvider.authorsVisible
		onVisibleChanged: viewID.setWidths()
		onWidthChanged: if (ready && viewID.width > 0 && width > 0) uiSettings.widthAuthor = width / viewID.width
		text: qsTranslate("Header", "Author")
	}

	CustomText
	{
		id: titleID
		function getIndex() { return uiSettings.indexTitle }
		property bool ready: false
		SplitView.preferredWidth: ready ? viewID.width * uiSettings.widthTitle : -1
		onWidthChanged: if (ready && viewID.width > 0 && width > 0) uiSettings.widthTitle = width / viewID.width
		text: qsTranslate("Header", "Title")
	}

	CustomText
	{
		id: seriesID
		function getIndex() { return uiSettings.indexSeries }
		property bool ready: false
		SplitView.preferredWidth: ready ? viewID.width * uiSettings.widthSeries : -1
		visible: fieldsVisibilityProvider.seriesVisible
		onVisibleChanged: viewID.setWidths()
		onWidthChanged: if (ready && viewID.width > 0 && width > 0) uiSettings.widthSeries = width / viewID.width
		text: qsTranslate("Header", "SeriesTitle")
	}

	TextMetrics
	{
		id: seqNoMetricsID
		font: seqNoID.font
		text: "9999"
	}

	CustomText
	{
		id: seqNoID
		function getIndex() { return uiSettings.indexSeqNo }
		property bool ready: false
		SplitView.preferredWidth: ready ? uiSettings.widthSeqNo : -1
		SplitView.minimumWidth: seqNoMetricsID.width + uiSettings.sizeSplitViewHandle
		horizontalAlignment: Text.AlignRight
		onWidthChanged: if (ready && viewID.width > 0 && width > 0) uiSettings.widthSeqNo = width
		text: qsTranslate("Header", "No")
	}

	CustomText
	{
		id: sizeID
		function getIndex() { return uiSettings.indexSize }
		property bool ready: false
		SplitView.preferredWidth: ready ? viewID.width * uiSettings.widthSize : -1
		horizontalAlignment: Text.AlignRight
		onWidthChanged: if (ready && viewID.width > 0 && width > 0) uiSettings.widthSize = width / viewID.width
		text: qsTranslate("Header", "Size")
	}

	CustomText
	{
		id: genreID
		function getIndex() { return uiSettings.indexGenre }
		property bool ready: false
		SplitView.preferredWidth: ready ? viewID.width * uiSettings.widthGenre : -1
		visible: fieldsVisibilityProvider.genresVisible
		onVisibleChanged: viewID.setWidths()
		onWidthChanged: if (ready && viewID.width > 0 && width > 0) uiSettings.widthGenre = width / viewID.width
		text: qsTranslate("Header", "GenreAlias")
	}

	CustomComboBox
	{
		id: langID
		function getIndex() { return uiSettings.indexLanguage }
		property bool ready: false
		SplitView.preferredWidth: ready ? uiSettings.widthLanguage : -1
		SplitView.minimumWidth: preferredWidth
		onWidthChanged: if (ready && viewID.width > 0 && width > 0) uiSettings.widthLanguage = width
		comboBoxController: guiController.GetLanguageComboBoxBooksController()
		translationContext: "DoNotTranslate"
	}

	function setWidths()
	{
		Functions.SetWidths([authorID, titleID, seriesID, seqNoID, sizeID, genreID, langID], viewID, function(item, value){ item.SplitView.fillWidth = value })
	}
}
