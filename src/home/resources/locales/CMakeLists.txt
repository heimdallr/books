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
set(LRELEASE_LIST)
set(LUPDATE_LIST)

foreach(locale ${locales})
    set(LOCALES_HEADER_CONTENT "${LOCALES_HEADER_CONTENT}\tQT_TRANSLATE_NOOP(\"Language\", \"${locale}\"),\n")
    set(QTBASE_QMFILE "${QT_TRANSLATIONS_DIR}/qtbase_${locale}.qm")
    set(QMFILE "${CMAKE_BINARY_DIR}/bin/locales/FLibrary_${locale}.qm")
    set(TSFILE "${CMAKE_CURRENT_LIST_DIR}/${locale}.ts")
    
	file(COPY ${QTBASE_QMFILE} DESTINATION "${CMAKE_BINARY_DIR}/bin/locales/")
	install(FILES ${QTBASE_QMFILE} ${QMFILE} DESTINATION "locales/")
    
    list(APPEND LRELEASE_LIST COMMAND "${QT_LRELEASE_TOOL}" "${TSFILE}" -qm "${QMFILE}")
    list(APPEND LUPDATE_LIST -ts "${TSFILE}")
endforeach()

configure_file("${BUILDSCRIPTS_ROOT}/helpers/locales.h.in" "${CMAKE_BINARY_DIR}/config/locales.h" @ONLY)

set(LUPDATE_COMMAND_PARAMETERS
	"${CMAKE_BINARY_DIR}/config/locales.h"
	"${FOUNDATION_ROOT}/util/language.h"
	"${FOUNDATION_ROOT}/util/GenresLocalization.h"
	./
	-recursive
	-locations none
	-extensions cpp,h,ui
)
if(QT6)
	list(APPEND LUPDATE_COMMAND_PARAMETERS -sort-messages)
endif()


set(LUPDATE_COMMAND
	"${QT_LUPDATE_TOOL}"
	${LUPDATE_COMMAND_PARAMETERS}
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
