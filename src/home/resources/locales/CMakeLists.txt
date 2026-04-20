cmake_minimum_required (VERSION 3.20)

set(EXECUTABLE_EXT)
if (WIN32)
	set(EXECUTABLE_EXT ".exe")
endif()

set(QT_LUPDATE_TOOL ${QT_BIN_DIR}/lupdate${EXECUTABLE_EXT})
if (NOT EXISTS ${QT_LUPDATE_TOOL})
	message(FATAL_ERROR "QT_LUPDATE_TOOL not found: ${QT_LUPDATE_TOOL}")
endif()
set(QT_LRELEASE_TOOL ${QT_BIN_DIR}/lrelease${EXECUTABLE_EXT})
if (NOT EXISTS ${QT_LRELEASE_TOOL})
	message(FATAL_ERROR "QT_LRELEASE_TOOL not found: ${QT_LRELEASE_TOOL}")
endif()
if (NOT EXISTS ${QT_TRANSLATIONS_DIR})
	message(FATAL_ERROR "QT_TRANSLATIONS_DIR not found: ${QT_TRANSLATIONS_DIR}")
endif()

set(locales en ru uk)

set(LOCALES_HEADER_CONTENT)
set(COPY_LIST)
set(LRELEASE_LIST)
set(LUPDATE_LIST)

foreach(locale ${locales})
    set(LOCALES_HEADER_CONTENT "${LOCALES_HEADER_CONTENT}\tQT_TRANSLATE_NOOP(\"Language\", \"${locale}\"),\n")
    list(APPEND COPY_LIST "${QT_TRANSLATIONS_DIR}/qtbase_${locale}.qm")
    list(APPEND LRELEASE_LIST COMMAND "${QT_LRELEASE_TOOL}" "${CMAKE_CURRENT_LIST_DIR}/${locale}.ts" -qm "${CMAKE_BINARY_DIR}/bin/locales/FLibrary_${locale}.qm")
    list(APPEND LUPDATE_LIST -ts "${CMAKE_CURRENT_LIST_DIR}/${locale}.ts")
    install(FILES "${CMAKE_BINARY_DIR}/bin/locales/FLibrary_${locale}.qm" DESTINATION "locales/")
endforeach()

configure_file("${BUILDSCRIPTS_ROOT}/helpers/locales.h.in" "${CMAKE_BINARY_DIR}/config/locales.h" @ONLY)
file(COPY ${COPY_LIST} DESTINATION "${CMAKE_BINARY_DIR}/bin/locales/")
install(FILES ${COPY_LIST} DESTINATION "locales/")

set(LUPDATE_COMMAND_PARAMETERS
	-recursive ./
	-locations none
)
if(QT6)
	list(APPEND LUPDATE_COMMAND_PARAMETERS -sort-messages)
endif()


set(LUPDATE_COMMAND
	"${QT_LUPDATE_TOOL}"
	"${CMAKE_BINARY_DIR}/config/locales.h"
	"${FOUNDATION_ROOT}/util/language.h"
	${LUPDATE_COMMAND_PARAMETERS}
	-extensions cpp,h,ui
	${LUPDATE_LIST}
)

execute_process(
    COMMAND ${LUPDATE_COMMAND}
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../../
)

add_custom_target(locales ${LRELEASE_LIST})
add_custom_target(locales_update_no_obsolete 
	COMMAND ${LUPDATE_COMMAND} -no-obsolete
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../../
)
