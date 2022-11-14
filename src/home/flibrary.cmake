CreateMap(UI_SETTING_NAMES
	VARIABLES
		colorBorder                   \"lightgray\"
		colorHighlight                \"lightsteelblue\"
		colorHighlightUnfocused       \"lavender\"
		colorProgressBar              \"lightsteelblue\"
		colorSplitViewHandle          \"\#c2f4c6\"
		colorSplitViewHandlePressed   \"\#81e889\"
		heightBookInfo                  0.25
		heightDelegate                  20
		heightMainWindow                720
		indexAuthor                     0
		indexTitle                      1
		indexSeries                     2
		indexSeqNo                      3
		indexGenre                      4
		indexLanguage                   5
		pathRecentCollectionDatabase  \"\"
		pathRecentExport              \"\"
		pathResentCollectionArchive   \"\"
		posXMainWindow                 -1
		posYMainWindow                 -1
		showBookInfo                    1
		showDeleted                     0
		sizeFont                        9
		sizeSplitViewHandle             4
		widthAuthor                     0.125
		widthGenre                      0.0625
		widthLanguage                   36
		widthMainWindow                 1024
		widthNavigation                 0.25
		widthSeqNo                      30
		widthSeries                     0.125
		widthTitle                      0.3
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
		DatabaseFactory
		DatabaseInt
		Fnd
		lzma
		MyHomeLibSQLIteExt
		SimpleSaxParser
		sqlite
		sqlite3pp
		Util
		ziplib
		zlib
		plog
	MODULES
		qt
	COMPILE_DEFINITIONS
		[ WIN32 PLOG_IMPORT ]
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)
