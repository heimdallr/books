CreateMap(UI_SETTING_NAMES
	VARIABLES
		showDeleted 0
		mainWindowHeight -1
		mainWindowWidth -1
		mainWindowPosX -1
		mainWindowPosY -1
	)

GenerateSettingsClass(UiSettings UI_SETTING_NAMES)

AddTarget(
	NAME flibrary
	TYPE app_bundle
	PROJECT_GROUP App
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/flibrary/src"
	INCLUDE_DIRS
		"${CMAKE_CURRENT_LIST_DIR}"
	SOURCES
		"${CMAKE_CURRENT_BINARY_DIR}/Settings/moc_UiSettings.cpp"
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
		DatabaseFactory
		DatabaseInt
		Fnd
		MyHomeLibSQLIteExt
		Util
		sqlite
		sqlite3pp
	MODULES
		qt
	COMPILER_OPTIONS
		[ MSVC /WX /W4 ]
)
