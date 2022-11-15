import QtQuick 2.12
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.15
import QtQuick.Window 2.12

import "qrc:/Core"
import "qrc:/Util/Functions.js" as Functions

Window
{
	id: dialogID
	width: uiSettings.heightRow * 30
	minimumHeight: collectionNameLayoutID.height + databaseFileNameLayoutID.height + collectionArchiveFolderLayoutID.height + buttonsLayoutID.height + 15 * Functions.GetMargin() + guiController.GetTitleBarHeight()
	maximumHeight: minimumHeight

	flags: Qt.Dialog

	title: qsTranslate("AddCollection", "Add new collection")

	ColumnLayout
	{
		id: layoutID

		anchors.fill: parent

		RowLayout
		{
			id: collectionNameLayoutID

			Layout.margins: Functions.GetMargin() * 2
			CustomText
			{
				Layout.preferredWidth: dialogID.width * uiSettings.widthAddCollectionDialogText
				text: qsTranslate("AddCollection", "Collection name")
			}
			TextField
			{
				id: collectionNameID
				Layout.fillWidth: true
				font.pointSize: uiSettings.sizeFont
			}
		}

		RowLayout
		{
			id: databaseFileNameLayoutID

			Layout.margins: Functions.GetMargin() * 2
			CustomText
			{
				Layout.preferredWidth: dialogID.width * uiSettings.widthAddCollectionDialogText
				text: qsTranslate("AddCollection", "Collection database file")
			}
			TextField
			{
				id: collectionDatabaseID
				Layout.fillWidth: true
				font.pointSize: uiSettings.sizeFont
			}
			Button
			{
			    text: qsTranslate("Common", "...")
				onClicked:
				{
					const fileName = fileDialog.SelectFile(collectionDatabaseID.text !== "" ? collectionDatabaseID.text : uiSettings.pathRecentCollectionDatabase)
					if (fileName === "")
						return

					uiSettings.pathRecentCollectionDatabase = fileName
					collectionDatabaseID.text = fileName
				}
			}
		}

		RowLayout
		{
			id: collectionArchiveFolderLayoutID

			Layout.margins: Functions.GetMargin() * 2
			CustomText
			{
				Layout.preferredWidth: dialogID.width * uiSettings.widthAddCollectionDialogText
				text: qsTranslate("AddCollection", "Collection archive folder")
			}
			TextField
			{
				id: collectionArchiveFolderID
				Layout.fillWidth: true
				font.pointSize: uiSettings.sizeFont
			}
			Button
			{
			    text: qsTranslate("Common", "...")
				onClicked:
				{
					const folder = fileDialog.SelectFolder(collectionArchiveFolderID.text !== "" ? collectionArchiveFolderID.text : uiSettings.pathResentCollectionArchive)
					if (folder === "")
						return

					uiSettings.pathResentCollectionArchive = folder
					collectionArchiveFolderID.text = folder
				}
			}
		}

		RowLayout
		{
			id: buttonsLayoutID

			Layout.margins: Functions.GetMargin() * 2

			Item
			{
				Layout.fillWidth: true
			}

			Button
			{
			    text: qsTranslate("Common", "Ok")
				onClicked:
				{
					collectionController.AddCollection(collectionNameID.text, collectionDatabaseID.text, collectionArchiveFolderID.text)
					dialogID.close()
				}
			}

			Button
			{
			    text: qsTranslate("Common", "Cancel")
				onClicked: dialogID.close()
			}
		}

		Item
		{
			Layout.fillHeight: true
		}
	}
}
