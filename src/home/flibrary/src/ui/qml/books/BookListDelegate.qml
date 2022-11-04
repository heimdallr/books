import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Item
{
	id: delegateID
	property bool authorsVisible: true
	property bool seriesVisible: true
	property bool genresVisible: true

	height: Constants.delegateHeight

	BookLayout
	{
		id: bookID
		anchors.fill: parent
		authorsVisible: delegateID.authorsVisible
		seriesVisible: delegateID.seriesVisible
		genresVisible: delegateID.genresVisible
	}
}
