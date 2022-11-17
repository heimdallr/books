import QtQuick 2.12
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

import Style 1.0

import "qrc:/Core"
import "qrc:/Util/Functions.js" as Functions

Window
{
	id: dialogID
	transientParent: applicationWindowID

	width: uiSettings.heightRow * 30
	minimumHeight: guiController.GetPixelMetric(Style.PM_TitleBarHeight)
		+ collectionNameLayoutID.height
		+ databaseFileNameLayoutID.height
		+ collectionArchiveFolderLayoutID.height
		+ buttonsLayoutID.height
		+ 16 * Functions.GetMargin()
	maximumHeight: minimumHeight

	flags: Qt.Dialog

	title: qsTranslate("AddCollection", "Add new collection")

	ColumnLayout
	{
		id: columnLayoutID

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
				onTextChanged: collectionController.error = ""
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
				Layout.preferredHeight: uiSettings.heightRow
				Layout.fillWidth: true
				font.pointSize: uiSettings.sizeFont
				text: uiSettings.pathRecentCollectionDatabase
				onTextChanged: collectionController.error = ""
			}
			Button
			{
				Layout.preferredHeight: uiSettings.heightRow
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
				Layout.preferredHeight: uiSettings.heightRow
				Layout.fillWidth: true
				font.pointSize: uiSettings.sizeFont
				text: uiSettings.pathResentCollectionArchive
				onTextChanged: collectionController.error = ""
			}
			Button
			{
				Layout.preferredHeight: uiSettings.heightRow
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
			Layout.fillWidth: true

			function closeDialog()
			{
				uiSettings.pathRecentCollectionDatabase = collectionDatabaseID.text
				uiSettings.pathResentCollectionArchive = collectionArchiveFolderID.text
				collectionController.addMode = false
			}

			Item
			{
				Layout.fillWidth: true
			}

			CustomText
			{
				Layout.preferredHeight: uiSettings.heightRow
				Layout.maximumWidth: buttonsLayoutID.width
					- btnCreateID.width
					- btnAddID.width
					- btnCancelID.width
					- Functions.GetMargin() * 10

				font.pointSize: uiSettings.sizeFont * uiSettings.sizeFontError
				color: uiSettings.colorErrorText
				text: collectionController.error
			}

			Button
			{
				id: btnCreateID
				Layout.preferredHeight: uiSettings.heightRow
			    text: qsTranslate("AddCollectionDialog", "Create new")
				onClicked: if (collectionController.CreateCollection(collectionNameID.text, collectionDatabaseID.text, collectionArchiveFolderID.text))
					buttonsLayoutID.closeDialog()
			}

			Button
			{
				id: btnAddID
				Layout.preferredHeight: uiSettings.heightRow
			    text: qsTranslate("AddCollectionDialog", "Add")
				onClicked: if (collectionController.AddCollection(collectionNameID.text, collectionDatabaseID.text, collectionArchiveFolderID.text))
					buttonsLayoutID.closeDialog()
			}

			Button
			{
				id: btnCancelID
				Layout.preferredHeight: uiSettings.heightRow
			    text: qsTranslate("Common", "Cancel")
				onClicked: collectionController.addMode = false
			}
		}
	}
}
