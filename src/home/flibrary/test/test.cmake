if(NOT QT6)
	return()
endif()

find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS Test)

# In-process GUI tests (Qt Test); not installed. Run with ctest, or directly: bin/gui_tests
AddTarget(gui_tests	app_console
	SKIP_INSTALL
	PROJECT_GROUP Test
	SOURCE_DIRECTORY
		"${CMAKE_CURRENT_LIST_DIR}/gui"
	COMPILE_DEFINITIONS
		FLIBRARY_TEST_FIXTURE_SCRIPT="${CMAKE_SOURCE_DIR}/src/home/tool/opds/test/make_fixture.py"
	LINK_LIBRARIES
		Boost::headers
		Qt${QT_MAJOR_VERSION}::Test
		Qt${QT_MAJOR_VERSION}::Widgets
	LINK_TARGETS
		flidjvu
		flint
		genre
		gui
		gutil
		logging
		logic
		platform
		settings
		util
)

add_test(NAME gui_tests COMMAND gui_tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set_tests_properties(gui_tests PROPERTIES TIMEOUT 300 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
