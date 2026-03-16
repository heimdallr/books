cmake_minimum_required (VERSION 3.20)

set(EXECUTABLE_EXT)
if (WIN32)
	set(EXECUTABLE_EXT ".exe")
endif()

set(QT_LUPDATE_TOOL ${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/lupdate${EXECUTABLE_EXT})
if (NOT EXISTS ${QT_LUPDATE_TOOL})
	message(FATAL_ERROR "QT_LUPDATE_TOOL not found: ${QT_LUPDATE_TOOL}")
endif()
set(QT_LRELEASE_TOOL ${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/lrelease${EXECUTABLE_EXT})
if (NOT EXISTS ${QT_LRELEASE_TOOL})
	message(FATAL_ERROR "QT_LRELEASE_TOOL not found: ${QT_LRELEASE_TOOL}")
endif()
set(QT_QMAKE_TOOL ${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/qmake6${EXECUTABLE_EXT})
if (NOT EXISTS ${QT_QMAKE_TOOL})
	message(FATAL_ERROR "QT_QMAKE_TOOL not found: ${QT_QMAKE_TOOL}")
endif()

if (NOT QT_TRANSLATIONS_DIR)
    execute_process(COMMAND ${QT_QMAKE_TOOL} -query QT_INSTALL_TRANSLATIONS OUTPUT_VARIABLE QT_TRANSLATIONS_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
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
endforeach()

configure_file("${BUILDSCRIPTS_ROOT}/helpers/locales.h.in" "${CMAKE_BINARY_DIR}/config/locales.h" @ONLY)
file(COPY ${COPY_LIST} DESTINATION "${CMAKE_BINARY_DIR}/bin/locales/")

set(LUPDATE_COMMAND
	"${QT_LUPDATE_TOOL}"
	"${CMAKE_BINARY_DIR}/config/locales.h"
	"${FOUNDATION_ROOT}/util/language.h"
	-recursive ./
	-locations none
	-sort-messages
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
