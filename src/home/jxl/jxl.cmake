AddTarget(JxlWrapper	shared_lib
	PROJECT_GROUP		Util
	SOURCE_DIRECTORY	"${CMAKE_CURRENT_LIST_DIR}"
	LINK_LIBRARIES
		libjxl::libjxl
		Qt${QT_MAJOR_VERSION}::Core
		Qt${QT_MAJOR_VERSION}::Gui
	LINK_TARGETS
		logging
)
