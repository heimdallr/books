import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import "qrc:/Core"

Item
{
	id: delegateID
	property bool authorsVisible: true
	property bool seriesVisible: true
	property bool genresVisible: true

	BookLayout
	{
		id: bookID
		anchors.fill: parent
		authorsVisible: delegateID.authorsVisible
		seriesVisible: delegateID.seriesVisible
		genresVisible: delegateID.genresVisible
	}
}
