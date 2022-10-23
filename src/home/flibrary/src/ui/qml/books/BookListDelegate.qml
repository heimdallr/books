import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import "qrc:/Core"
import "qrc:/Core/constants.js" as Constants

Rectangle
{
	id: delegateID
	property bool authorsVisible: true
	property bool seriesVisible: true
	property bool genresVisible: true

	height: Constants.delegateHeight
	color: "transparent"

	border { color: Constants.borderColor; width: 1 }

	BookLayout
	{
		id: bookID
		anchors.fill: parent
		authorsVisible: delegateID.authorsVisible
		seriesVisible: delegateID.seriesVisible
		genresVisible: delegateID.genresVisible
	}
}
