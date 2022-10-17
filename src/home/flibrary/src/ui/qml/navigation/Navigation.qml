import QtQuick 2.15

import "Core"

CustomListView
{
	modelController: guiController.GetAuthorsModelController()
	delegate: AuthorDelegate {}
}
