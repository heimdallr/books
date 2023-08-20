import QtQuick 2.12
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.1
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

import Style 1.0

import "qrc:/Core"
import "qrc:/Util/Functions.js" as Functions

Window
{
	id: dialogID

	readonly property var collectionController: guiController.GetCollectionController()

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

	onVisibleChanged: if (visible)
	{
		collectionNameID.focus = true
		collectionNameID.selectAll()
	}

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
					const fileName = fileDialog.SaveFile(qsTranslate("FileDialog", "Select database file"), collectionDatabaseID.text !== "" ? collectionDatabaseID.text : uiSettings.pathRecentCollectionDatabase, qsTranslate("FileDialog", "Flibrary database files (*.db *.db3 *.s3db *.sl3 *.sqlite *.sqlite3 *.hlc *.hlc2);;All files (*.*)"))
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
					const folder = fileDialog.SelectFolder(qsTranslate("FileDialog", "Select archives folder"), collectionArchiveFolderID.text !== "" ? collectionArchiveFolderID.text : uiSettings.pathResentCollectionArchive)
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
				collectionController.GetAddModeDialogController().visible = false
			}

			CustomText
			{
				Layout.preferredHeight: uiSettings.heightRow
				Layout.fillWidth: true

				horizontalAlignment: Text.AlignRight
				font.pointSize: uiSettings.sizeFont * uiSettings.sizeFontError
				color: uiSettings.colorErrorText
				text: collectionController.error
			}

			Button
			{
				MessageDialog
				{
				    id: overrideConfirmDialogID
				    title: qsTranslate("Common", "Warning")
				    text: qsTranslate("AddCollectionDialog", "Database file already exists. Owerride?")
					standardButtons: StandardButton.Yes | StandardButton.No
					icon: StandardIcon.Warning
				    onYes: collectionController.CreateCollection(collectionNameID.text, collectionDatabaseID.text, collectionArchiveFolderID.text)
				}

				id: btnCreateID
				Layout.preferredHeight: uiSettings.heightRow
			    text: qsTranslate("AddCollectionDialog", "Create new")
				onClicked: if (collectionController.CheckCreateCollection(collectionNameID.text, collectionDatabaseID.text, collectionArchiveFolderID.text))
				{
					buttonsLayoutID.closeDialog()
					if (fileDialog.FileExists(collectionDatabaseID.text))
						overrideConfirmDialogID.open()
					else
						collectionController.CreateCollection(collectionNameID.text, collectionDatabaseID.text, collectionArchiveFolderID.text)
				}
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
				enabled: collectionController.CollectionsCount() > 0
				onClicked: collectionController.GetAddModeDialogController().visible = false
			}
		}
	}
}
