import QtQuick 2.12
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.15
import QtQuick.Window 2.12

import "qrc:/Core"

Window
{
	id: dialogID
	width: 600
	minimumHeight: 180
	maximumHeight: 180

	flags: Qt.Dialog

	title: qsTranslate("AddCollection", "Add new collection")

	ColumnLayout
	{
		id: layoutID

		anchors.fill: parent

		RowLayout
		{
			Layout.margins: 8
			CustomText
			{
				Layout.preferredWidth: 150
				text: qsTranslate("AddCollection", "Collection name")
			}
			TextField
			{
				id: collectionNameID
				Layout.fillWidth: true
				font.pointSize: uiSettings.fontSize
			}
		}

		RowLayout
		{
			Layout.margins: 8
			CustomText
			{
				Layout.preferredWidth: 150
				text: qsTranslate("AddCollection", "Collection database file")
			}
			TextField
			{
				id: collectionDatabaseID
				Layout.fillWidth: true
				font.pointSize: uiSettings.fontSize
			}
			Button
			{
			    text: qsTranslate("Common", "...")
				onClicked:
				{
					const fileName = fileDialog.SelectFile(collectionDatabaseID.text !== "" ? collectionDatabaseID.text : uiSettings.recentCollectionDatabase)
					if (fileName === "")
						return

					uiSettings.recentCollectionDatabase = fileName
					collectionDatabaseID.text = fileName
				}
			}
		}

		RowLayout
		{
			Layout.margins: 8
			CustomText
			{
				Layout.preferredWidth: 150
				text: qsTranslate("AddCollection", "Collection archive folder")
			}
			TextField
			{
				id: collectionArchiveFolderID
				Layout.fillWidth: true
				font.pointSize: uiSettings.fontSize
			}
			Button
			{
			    text: qsTranslate("Common", "...")
				onClicked:
				{
					const folder = fileDialog.SelectFolder(collectionArchiveFolderID.text !== "" ? collectionArchiveFolderID.text : uiSettings.resentCollectionArchiveFolder)
					if (folder === "")
						return

					uiSettings.resentCollectionArchiveFolder = folder
					collectionArchiveFolderID.text = folder
				}
			}
		}

		RowLayout
		{
			Layout.margins: 8

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
