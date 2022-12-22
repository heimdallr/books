import QtQuick 2.15
import QtQuick.Layouts 1.15

import "qrc:/Core"

ColumnLayout
{
	id: booksViewID

	spacing: 0

	BooksHeader
	{
		Layout.preferredHeight: uiSettings.heightRow
		Layout.fillWidth: true
	}

	Line
	{
		Layout.fillWidth: true
	}

	CustomListView
	{
		Layout.fillHeight: true
		Layout.fillWidth: true

		delegate: BookTreeDelegate
		{
			height: uiSettings.heightRow
			width: booksViewID.width
		}
	}
}
