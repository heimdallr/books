CreateMap(UI_SETTING_NAMES
	VARIABLES
		colorBackground               \"white\"
		colorBookText                 \"black\"
		colorBookTextDeleted          \"gray\"
		colorBorder                   \"lightgray\"
		colorErrorText                \"red\"
		colorHighlight                \"lightsteelblue\"
		colorHighlightUnfocused       \"lavender\"
		colorLine                     \"black\"
		colorLogDebug                 \"darkgrey\"
		colorLogDefault               \"blue\"
		colorLogError                 \"red\"
		colorLogFatal                 \"darkred\"
		colorLogInfo                  \"black\"
		colorLogVerbose               \"lightgray\"
		colorLogWarning               \"magenta\"
		colorProgressBar              \"lightsteelblue\"
		colorSplitViewHandle          \"white\"
		colorSplitViewHandleHovered   \"\#d0d0d0\"
		colorSplitViewHandlePressed   \"\#c0c0c0\"
		heightBookInfo                  0.25
		heightMainWindow                720
		heightRow                       20
		indexAuthor                     0
		indexTitle                      1
		indexSeries                     2
		indexSeqNo                      3
		indexSize                       4
		indexGenre                      5
		indexLanguage                   6
		locale                        \"\"
		logLevel                      \"4\"
		pathRecentCollectionDatabase  \"\"
		pathRecentExport              \"\"
		pathResentCollectionArchive   \"\"
		posXMainWindow                 -1
		posYMainWindow                 -1
		showBookInfo                    1
		showDeleted                     0
		sizeCheckbox                    0.8
		sizeExpander                    0.4
		sizeFont                        9
		sizeFontError                   0.9
		sizeFontToolTip                 0.9
		sizeLogMaximum                  10000
		sizeScrollbar                   0.8
		sizeSplitViewHandle             4
		toolTipDelay                    400
		toolTipTimeout                  4000
		viewModeBooks                 \"Find\"
		viewModeNavigation            \"Find\"
		viewModeValueBooks            \"\"
		viewModeValueNavigation       \"\"
		viewSourceBooks               \"BooksListView\"
		viewSourceNavigation          \"Authors\"
		widthAddCollectionDialogText    0.3
		widthAuthor                     0.125
		widthGenre                      0.0625
		widthLanguage                   36
		widthMainWindow                 1024
		widthNavigation                 0.25
		widthSeqNo                      30
		widthSeries                     0.125
		widthSize                       0.2
		widthTitle                      0.3
		widthTreeMargin                 0.75
	)

GenerateSettingsClass(UiSettings UI_SETTING_NAMES)


set(LOCALES
	"en"
	"ru"
	)
GenerateTranslations(
	NAME "flibrary"
	PATH "${CMAKE_CURRENT_LIST_DIR}/flibrary"
	LOCALES ${LOCALES}
	)

AddTarget(
	NAME flibrary
	TYPE app_bundle
	WIN_APP_ICON "${CMAKE_CURRENT_LIST_DIR}/flibrary/resources/icons/main.ico"
	PROJECT_GROUP App
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/flibrary/src"
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}"
		"${CMAKE_CURRENT_LIST_DIR}/../ext/ziplib/Source"
		"${CMAKE_CURRENT_LIST_DIR}/../ext"
		"${CMAKE_CURRENT_LIST_DIR}/../ext/plog/include"
		"${CMAKE_CURRENT_LIST_DIR}/inpx/src"
	SOURCES
		"${CMAKE_CURRENT_BINARY_DIR}/Settings/moc_UiSettings.cpp"
	QRC
		"${CMAKE_CURRENT_BINARY_DIR}/Resources/flibrary_qm.qrc"
	QT_USE
		Core
		Widgets
		Gui
		Qml
		Quick
		QuickControls2
		QuickTemplates2
	QT_QML_MODULES
		QtQml
		QtQuick
		QtQuick.2
		QtGraphicalEffects
		Qt
	QT_PLUGINS
		QJpeg
	QRC
		"${CMAKE_CURRENT_LIST_DIR}/flibrary/resources/main.qrc"
	LINK_TARGETS
		bzip2
		lzma
		ziplib
		zlib
		sqlite
		sqlite3pp
		DatabaseFactory
		DatabaseInt
		Fnd
		InpxLib
		MyHomeLibSQLIteExt
		plog
		Util
	MODULES
		qt
	COMPILE_DEFINITIONS
		[ WIN32 PLOG_IMPORT ]
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)
