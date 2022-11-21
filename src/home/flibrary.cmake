CreateMap(UI_SETTING_NAMES
	VARIABLES
		colorBackground               \"white\"
		colorBorder                   \"lightgray\"
		colorErrorText                \"red\"
		colorHighlight                \"lightsteelblue\"
		colorHighlightUnfocused       \"lavender\"
		colorLogFatal                 \"darkred\"
		colorLogError                 \"red\"
		colorLogWarning               \"magenta\"
		colorLogInfo                  \"black\"
		colorLogDebug                 \"darkgrey\"
		colorLogVerbose               \"lightgray\"
		colorLogDefault               \"blue\"
		colorProgressBar              \"lightsteelblue\"
		colorSplitViewHandle          \"\#c2f4c6\"
		colorSplitViewHandlePressed   \"\#81e889\"
		heightBookInfo                  0.25
		heightMainWindow                720
		heightRow                       20
		indexAuthor                     0
		indexTitle                      1
		indexSeries                     2
		indexSeqNo                      3
		indexGenre                      4
		indexLanguage                   5
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
		sizeLogMaximum                  10000
		sizeScrollbar                   0.8
		sizeSplitViewHandle             4
		viewModeBooks                 \"Find\"
		viewModeNavigation            \"Find\"
		viewModeValueBooks            \"\"
		viewModeValueNavigation       \"\"
		widthAddCollectionDialogText    0.3
		widthAuthor                     0.125
		widthGenre                      0.0625
		widthLanguage                   36
		widthMainWindow                 1024
		widthNavigation                 0.25
		widthSeqNo                      30
		widthSeries                     0.125
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
		SimpleSaxParser
		Util
	MODULES
		qt
	COMPILE_DEFINITIONS
		[ WIN32 PLOG_IMPORT ]
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)
