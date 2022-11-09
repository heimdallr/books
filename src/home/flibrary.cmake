CreateMap(UI_SETTING_NAMES
	VARIABLES
		showDeleted               0
		showBookInfo              1
		mainWindowHeight         -1
		mainWindowWidth          -1
		mainWindowPosX           -1
		mainWindowPosY           -1
		navigationWidth           0.25
		bookInfoHeight            0.25
		delegateHeight            20
		fontSize                  9
		borderColor             \"lightgray\"
		highlightColor          \"lightsteelblue\"
		highlightColorUnfocused \"lavender\"
		progressBarColor        \"lightsteelblue\"
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
