include_guard(GLOBAL)

include(CMakeParseArguments)

set(CMAKE_DEBUG_POSTFIX "D")
set(SCRIPT_HELPERS_DIR "${CMAKE_CURRENT_LIST_DIR}/../helpers")

# BIN_DIR можеть быть задан в параметрах генерации
if(NOT BIN_DIR)
	set(BIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/bin)
else()
	string(REPLACE "\\" "/" BIN_DIR ${BIN_DIR})
endif()
message(STATUS "Using directory ${BIN_DIR} for output")

#######################################################
# Вспомогательные функции/макросы для AddTarget #
#######################################################

# Сброс всех текущих переменных.
macro(initProject)
	set( EXTRA_INCLUDE_DIRECTORIES )
	set( EXTRA_LINK_DIRECTORIES )
	set( EXTRA_LINK_LIBRARIES )
	set( EXTRA_LINK_LIBRARIES_DEBUG )
	set( EXTRA_LINK_LIBRARIES_RELEASE )
	set( EXTRA_LINK_FLAGS )
	set( EXTRA_PRIVATE_DEFINITIONS )
	set( EXTRA_COMPILE_FLAGS )
	set( EXTRA_WORKSPACE_FILES )
	set( EXTRA_PLUGINS )
	set( EXTRA_PLUGINS_RELEASE )
	set( EXTRA_PLUGINS_DEBUG )
	set( EXTRA_DEBUGINFO_DEBUG )
	set( EXTRA_DEBUGINFO_RELEASE )
	set( EXTRA_MACOS_FRAMEWORKS )
endmacro()

#  Макрос составляет группы исходников аналогично их расположению на диске относительно директории
# ARG_SOURCE_DIR. На входе имеет CURRENT_SOURCES, CURRENT_HEADERS, CURRENT_QML. Применимо для VS.
macro(makeSourceGroups)
	GET_FILENAME_COMPONENT( CURRENT_PROJECT_FULLPATH ${ARG_SOURCE_DIR} ABSOLUTE )
	FOREACH( SOURCE ${CURRENT_SOURCES} )
		GET_FILENAME_COMPONENT( FULLPATH ${SOURCE} ABSOLUTE )
		GET_FILENAME_COMPONENT( SOURCE_NAME ${SOURCE} NAME )
		STRING( REPLACE ${SOURCE_NAME} "" FULLPATH "${FULLPATH}" )
		STRING( REPLACE ${CURRENT_PROJECT_FULLPATH} "" RELATIVEPATH "${FULLPATH}" )
		STRING( REPLACE "/" "\\" BACKSLASHEDPATH "${RELATIVEPATH}" )
		SOURCE_GROUP( "Source Files${BACKSLASHEDPATH}" FILES ${SOURCE} )
	ENDFOREACH( SOURCE )

	FOREACH( HEADER ${CURRENT_HEADERS} )
		GET_FILENAME_COMPONENT( FULLPATH ${HEADER} ABSOLUTE )
		GET_FILENAME_COMPONENT( HEADER_NAME ${HEADER} NAME )
		STRING( REPLACE ${HEADER_NAME} "" FULLPATH "${FULLPATH}" )
		STRING( REPLACE ${CURRENT_PROJECT_FULLPATH} "" RELATIVEPATH "${FULLPATH}" )
		STRING( REPLACE "/" "\\" BACKSLASHEDPATH "${RELATIVEPATH}" )
		SOURCE_GROUP( "Header Files${BACKSLASHEDPATH}" FILES ${HEADER} )
	ENDFOREACH( HEADER )

	FOREACH( QML ${CURRENT_QML} )
		GET_FILENAME_COMPONENT( FULLPATH ${QML} ABSOLUTE )
		GET_FILENAME_COMPONENT( QML_NAME ${QML} NAME )
		STRING( REPLACE ${QML_NAME} "" FULLPATH "${FULLPATH}" )
		STRING( REPLACE ${CURRENT_PROJECT_FULLPATH} "" RELATIVEPATH "${FULLPATH}" )
		STRING( REPLACE "/" "\\" BACKSLASHEDPATH "${RELATIVEPATH}" )
		SOURCE_GROUP( "QML Files${BACKSLASHEDPATH}" FILES ${QML} )
	ENDFOREACH( QML )

	FOREACH( FORM ${CURRENT_FORMS} )
		GET_FILENAME_COMPONENT( FULLPATH ${FORM} ABSOLUTE )
		GET_FILENAME_COMPONENT( FORM_NAME ${FORM} NAME )
		STRING( REPLACE ${FORM_NAME} "" FULLPATH "${FULLPATH}" )
		STRING( REPLACE ${CURRENT_PROJECT_FULLPATH} "" RELATIVEPATH "${FULLPATH}" )
		STRING( REPLACE "/" "\\" BACKSLASHEDPATH "${RELATIVEPATH}" )
		SOURCE_GROUP( "Form Files${BACKSLASHEDPATH}" FILES ${FORM} )
	ENDFOREACH( FORM )
endmacro()

# Фильтрует список файлов listToFilter в зависимости от платформы
# Так же можно указать REGEXP дополнительным параметром, тогда из списка будут удалены все элементы соответствующие ему
function(filterSources listToFilter)
	set(regexList ${ARGN})
	if( NOT WIN32 )
		append(regexList "(/[Ww]in/|_[Ww]in[.]|_[Ww]in/)") #todo: не использовать директории NativeWebView_mac/NativeWebView_win
	endif()
	if(NOT APPLE)
		append(regexList "(/[Mm]ac/|_[Mm]ac[.]|_[Mm]ac/)")
	endif()
	if(NOT ANDROID)
		append(regexList "(/[Aa]ndroid/|_[Aa]ndroid[.])")
	endif()
	if(NOT LINUX)
		append(regexList "(/[Ll]inux/|_[Ll]inux[.])")
	endif()
	if(NOT LINUX OR ANDROID)
		append(regexList "(/[Ll]inux_os/|_[Ll]inux_os[.])") # Special case for separating android and linux distributions
	endif()
	if(NOT UNIX)
		append(regexList "(/[Uu]nix/|_[Uu]nix[.])")
	endif()

	string(REPLACE ";" "|" regexList "(${regexList})") # склеиваем все регулярки в одну по "или". Как минимум одна у нас всегда будет.
	list(FILTER ${listToFilter} EXCLUDE REGEX "${regexList}")

	set( ${listToFilter} ${${listToFilter}} PARENT_SCOPE)

endfunction(filterSources)


# Вызывается после add_executable или add_library.
# Устанавливает свойства проекта TARGET_NAME, заданные переменными EXTRA_*
function(postTarget TARGET_NAME)
	# Установка директорий заголовочных файлов
	target_include_directories(${TARGET_NAME} PRIVATE ${EXTRA_INCLUDE_DIRECTORIES})

	if( WIN32 )
		# выставляем дополнительные пути для компоновщика.
		append( EXTRA_LINK_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR}  )
		set(_link_flags)
		#эмулируем поведение для link_directories(), но только для одного проекта.
		foreach(FLINK_DIR ${EXTRA_LINK_DIRECTORIES})
			if (FLINK_DIR MATCHES " ")
				# примечание: можно использовать конструкцию -LIBPATH:"", но Visual Studio  родной генератор некорректно обрабатывает такие пути.
				message(SEND_ERROR "LINK_DIRECTORIES should not contain spaces! FLINK_DIR=${FLINK_DIR}")
			endif()
			append(_link_flags "-LIBPATH:${FLINK_DIR}")
		endforeach()
		set(flagScope PRIVATE)
		if (${ARG_TYPE} STREQUAL static_lib)
			set(flagScope PUBLIC)
		endif()
		target_link_libraries(${TARGET_NAME} ${flagScope} ${_link_flags})
	endif()

	if (MSVC AND ARG_STATIC_RUNTIME)
		append(EXTRA_LINK_LIBRARIES optimized libcmt.lib debug libcmtd.lib)
		target_compile_options(${TARGET_NAME} PRIVATE $<$<CONFIG:Debug>:/MTd>$<$<CONFIG:Release>:/MT>)
	endif()

	# Флаги компиляции
	foreach(FCOMPILE_FLAG ${EXTRA_COMPILE_FLAGS})
		target_compile_options(${TARGET_NAME} PRIVATE ${FCOMPILE_FLAG})
	endforeach()

	FOREACH(FPRIVATE_DEF ${EXTRA_PRIVATE_DEFINITIONS})
		TARGET_COMPILE_DEFINITIONS( ${TARGET_NAME} PRIVATE ${FPRIVATE_DEF})
	ENDFOREACH()

	IF( APPLE )
		SET_TARGET_PROPERTIES(${TARGET_NAME} PROPERTIES XCODE_ATTRIBUTE_CLANG_LINK_OBJC_RUNTIME "NO")
	ENDIF()

	TARGET_LINK_LIBRARIES( ${TARGET_NAME} LINK_PRIVATE ${EXTRA_LINK_LIBRARIES} )

	# Флаги компоновки
	if( EXTRA_LINK_FLAGS )
		string(REPLACE ";" " " EXTRA_LINK_FLAGS "${EXTRA_LINK_FLAGS}")
		set_target_properties( ${ARG_NAME} PROPERTIES LINK_FLAGS " ${EXTRA_LINK_FLAGS} " )
	endif()

	# по дефолту всегда генерим PDB в Release.
	if (MSVC AND NOT (TargetType STREQUAL "STATIC") AND NOT WIN_DISABLE_RELEASE_PDB AND NOT ARG_NO_DEBUG_SYMBOLS)
		set_target_properties( ${ARG_NAME} PROPERTIES LINK_FLAGS_RELEASE " /DEBUG " )
	endif()
endfunction()

# Устанавливает проекту TARGET_NAME папку, используется в основном для студии.
# Пример: setProjectFolder(GuiInt Gui)
function(setProjectFolder TARGET_NAME FOLDER)
	SET_PROPERTY(TARGET ${TARGET_NAME} PROPERTY FOLDER ${FOLDER})
endfunction()

function(setTargetOutput TARGET_NAME TARGET_TYPE OUTPUT_NAME)
	set(typeList shared_lib app app_bundle app_console)
	if( ${TARGET_TYPE} IN_LIST typeList )

		if( ${TARGET_TYPE} STREQUAL shared_lib )
			set(file_suffix ${CMAKE_SHARED_LIBRARY_SUFFIX})
			set(file_prefix ${CMAKE_SHARED_LIBRARY_PREFIX})
		else()
			set(file_suffix ${CMAKE_EXECUTABLE_SUFFIX})
			set(file_prefix)
		endif()

		set_target_properties( ${TARGET_NAME} PROPERTIES OUTPUT_NAME "${OUTPUT_NAME}" )
		if( WIN32 )
			# Под win все dll и exe складывается в директорию bin, дебажные помечаются постфиксом ${CMAKE_DEBUG_POSTFIX}
			# RELEASE  и DEBUG для генератора VS
			set_target_properties( ${TARGET_NAME} PROPERTIES
				RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}/${ARG_OUTPUT_SUBDIRECTORY}
				RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR}/${ARG_OUTPUT_SUBDIRECTORY}
			)
			set_target_properties( ${TARGET_NAME} PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX} )
			set( RUNTIME_FILE_${TARGET_NAME}_Debug "${file_prefix}${OUTPUT_NAME}${CMAKE_DEBUG_POSTFIX}${file_suffix}" CACHE FILEPATH "" FORCE )
			set( RUNTIME_FILE_${TARGET_NAME}_Release "${file_prefix}${OUTPUT_NAME}${file_suffix}" CACHE FILEPATH "" FORCE )
		else()
			if(LINUX AND NOT ANDROID)
				set_target_properties( ${TARGET_NAME} PROPERTIES
					LIBRARY_OUTPUT_DIRECTORY ${BIN_DIR}/${ARG_OUTPUT_SUBDIRECTORY}
					RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR}/${ARG_OUTPUT_SUBDIRECTORY}
				)
			endif()
			# Как называются библиотеки на мак после сборки перед тем как попасть в бандл?
			set( RUNTIME_FILE_${TARGET_NAME}_Debug "${file_prefix}${OUTPUT_NAME}${file_suffix}" CACHE FILEPATH "" FORCE )
			set( RUNTIME_FILE_${TARGET_NAME}_Release "${file_prefix}${OUTPUT_NAME}${file_suffix}" CACHE FILEPATH "" FORCE )
		endif()
		mark_as_advanced(RUNTIME_FILE_${TARGET_NAME}_Debug RUNTIME_FILE_${TARGET_NAME}_Release)
	endif()
endfunction(setTargetOutput TARGET_NAME)

function(CreateExportLibFile target definition_name output_file)
	set(TARGET ${target})
	set(DEFINITION_NAME ${definition_name})
	configure_file(${SCRIPT_HELPERS_DIR}/ExportLib.h.in ${output_file} @ONLY)
endfunction()

function(PrepareTargetSources SourcesList ExtraIncludes)
	file(GLOB_RECURSE allFiles "${ARG_SOURCE_DIR}/[^.]*" ) # Пропуск скрытых файлов
	set(CURRENT_HEADERS ${allFiles})
	list(FILTER CURRENT_HEADERS INCLUDE REGEX "\\.h$")
	set(CURRENT_SOURCES ${allFiles})
	list(FILTER CURRENT_SOURCES INCLUDE REGEX "\\.(c|cc|cpp|mm|m)$")
	set(CURRENT_QML ${allFiles})
	list(FILTER CURRENT_QML INCLUDE REGEX "\\.(qml|js)$")
	if(ARG_QT)
		set(CURRENT_FORMS ${allFiles})
		list(FILTER CURRENT_FORMS INCLUDE REGEX "\\.ui$")
		set(CURRENT_RESOURCES ${allFiles})
		list(FILTER CURRENT_RESOURCES INCLUDE REGEX "\\.qrc$")
		if (ARG_FORMS)
			list(APPEND CURRENT_FORMS ${ARG_FORMS})
		endif()
		if (ARG_QRC)
			list(APPEND CURRENT_RESOURCES ${ARG_QRC})
		endif()
		foreach(qrc_file ${ARG_QRC})
			message( STATUS "${ARG_NAME}: Using additional qrc: ${qrc_file}" )
			check_if_exists(${qrc_file} "${ARG_NAME}: ${qrc_file} file specified in QRC list not exists")
		endforeach()
		filterSources(CURRENT_FORMS ${ARG_EXCLUDE_SOURCES})
		filterSources(CURRENT_RESOURCES ${ARG_EXCLUDE_SOURCES})
	endif()

	if (WIN32 AND ARG_WIN_APP_ICON)
		append( CURRENT_SOURCES "${ARG_NAME}/win_resources.rc" )
	endif()

	if (WIN32)
		foreach(rc_file ${ARG_WIN_RC})
			message( STATUS "${ARG_NAME}: Using additional win rc: ${rc_file}" )
			append(CURRENT_SOURCES ${rc_file})
		endforeach()
	endif()

	filterSources(CURRENT_SOURCES ${ARG_EXCLUDE_SOURCES})
	filterSources(CURRENT_HEADERS ${ARG_EXCLUDE_SOURCES})
	filterSources(CURRENT_QML     ${ARG_EXCLUDE_SOURCES})

	if (CMAKE_GENERATOR MATCHES "Visual Studio")
		makeSourceGroups()
	endif()

	set(ALL_SOURCES ${CURRENT_SOURCES} ${CURRENT_HEADERS} ${CURRENT_QML} ${ARG_SOURCES})
	if(ARG_QT)
		set(hasDesigner false)
		if (Designer IN_LIST ARG_QT_USE)
			set(hasDesigner true)
		endif()
		preTargetQt(${hasDesigner} "${ARG_INCLUDE_DIRS}" CURRENT_QT_FILES ExtraQtIncludes CURRENT_SOURCES_MOC)
		set(${ExtraIncludes} ${${ExtraIncludes}} ${ExtraQtIncludes} PARENT_SCOPE)
		append(ALL_SOURCES ${CURRENT_QT_FILES})
	endif()

	if (APPLE)
		foreach(xib ${ARG_MAC_XIB})
			get_filename_component(__namewe "${xib}" NAME_WE)
			set(__outfile ${CMAKE_CURRENT_BINARY_DIR}/${__namewe}.nib)
			add_custom_command (
				OUTPUT ${__outfile}
				MAIN_DEPENDENCY ${xib}
				COMMAND ${IBTOOL} --errors --warnings --notices --output-format human-readable-text --compile ${__outfile} ${xib}
				COMMENT "Compiling ${xib}")
			append_unique(ALL_SOURCES "${__outfile}")
		endforeach()
	endif()

	# Объединенные сборки
	if (NOT ARG_AMALGAMATION_MODE)
		set(ARG_AMALGAMATION_MODE moc)
	endif()
	if (ARG_AMALGAMATION_MODE STREQUAL "moc" OR ARG_AMALGAMATION_MODE STREQUAL "all" )
		set(miminalFilesCount 2) # miminalFilesCount + 1 = сколько файлов минимально должно быть в списке для амальгамирования
		set(amalgamationBuildFile ${CMAKE_CURRENT_BINARY_DIR}/amalgamation/amalg_${ARG_NAME}.cpp)
		set(fileMask ".*\\.cpp")
		set(amalgSources ${ALL_SOURCES})
		if (ARG_AMALGAMATION_MODE STREQUAL "moc")
			set(amalgSources ${CURRENT_SOURCES_MOC})
			set(fileMask ".*moc_.*\\.cpp")
		endif()
		if (amalgSources)
			list(FILTER amalgSources INCLUDE REGEX "${fileMask}")
		endif()
		list(LENGTH amalgSources amalgSourcesSize)
		if (amalgSourcesSize GREATER miminalFilesCount)
			string(REPLACE ";" ">\n#include <" amalgamation_lines "${amalgSources}")
			set(amalgamation_lines "#include <${amalgamation_lines}>\n")
			configure_file(${SCRIPT_HELPERS_DIR}/AmalgamationBuild.cpp.in "${amalgamationBuildFile}")
			set_source_files_properties(${amalgSources} PROPERTIES HEADER_FILE_ONLY true)
			append_unique(ALL_SOURCES "${amalgamationBuildFile}")
		endif()
	endif()

	append(ALL_SOURCES ${EXTRA_TARGET_SOURCES} ${CMAKE_CURRENT_LIST_FILE})

	set(${SourcesList} ${ALL_SOURCES} PARENT_SCOPE)
endfunction()

function(GenerateQtConf output_path)
	set(PATH_IN_BUNDLE "${output_path}")

	if(PATH_IN_BUNDLE)
		set(PATH_IN_BUNDLE "/${PATH_IN_BUNDLE}")
	endif()

	if(CMAKE_GENERATOR MATCHES Xcode)
		foreach (type ${CMAKE_CONFIGURATION_TYPES})
			configure_file( ${SCRIPT_HELPERS_DIR}/qt.conf.nofixup.in ${CMAKE_CURRENT_BINARY_DIR}/${type}${PATH_IN_BUNDLE}/qt.conf @ONLY )
		endforeach()
	else()
		set(bundleBinaryPath "${CMAKE_CURRENT_BINARY_DIR}${PATH_IN_BUNDLE}")
		configure_file( ${SCRIPT_HELPERS_DIR}/qt.conf.nofixup.in ${bundleBinaryPath}/qt.conf @ONLY )
	endif()
endfunction()

function(GenerateNoFixupFile bundle_name)
	set(NOFIXUP_FILE "${bundle_name}/Contents/Resources/nofixup")
	if(CMAKE_GENERATOR MATCHES Xcode)
		foreach (type ${CMAKE_CONFIGURATION_TYPES})
			file(TOUCH ${CMAKE_CURRENT_BINARY_DIR}/${type}/${NOFIXUP_FILE})
		endforeach()
	else()
		file(TOUCH ${CMAKE_CURRENT_BINARY_DIR}/${NOFIXUP_FILE})
	endif()
endfunction()

# Создание новой цели
function(AddTarget)
	set(__options
		QT                  # используется Qt
		NEED_PROTECTION     # таргет необходимо защищать (только для Win)
		EXPORT_INCLUDE      # выставлять текущую директорию цели как публичный include при её компоновке.
		STATIC_RUNTIME      # использовать static runtime (/MT) стандартной библиотеки (только MSVC)
		NO_DEBUG_SYMBOLS    # отключить создание PDB для цели (даже если они включены глобально)
		)
	set(__one_val_required
		NAME                # Имя цели, обязательно
		TYPE                # Тип цели shared_lib|static_lib|app|app_bundle|app_console|header_only
		SOURCE_DIR          # Путь к папке с исходниками
		)
	set(__one_val_optional
		PROJECT_GROUP       # Группа проекта, может быть составной "Proc/Codec"
		OUTPUT_NAME         # Задаёт выходное имя таргета
		OUTPUT_SUBDIRECTORY # Путь к папке относительно BIN_DIR
		BUNDLE_INFO_PLIST   # Путь к Info.plist для bundle
		BUNDLE_ICONS        # Путь к icns для bundle
		ENTITLEMENTS        # Entitlement с которыми будет подписан таргет для AppStore, либо для активации hardened runtime
		WIN_APP_ICON        # Путь до иконки приложения (win)
		AMALGAMATION_MODE   # тип объединенной сборки. all|moc|none , all = все исходники объединяются, moc - только moc-файлы. По умолчанию - moc.
		)
	set(__multi_val
		COMPILER_OPTIONS             # дополнительные опции для компилятора
		COMPILE_DEFINITIONS          # дополнительные дефайны препроцессора (без -D)
		EXCLUDE_SOURCES              # регулярное выражение для исключения исходников. E.g. "blabla\\.(cpp|h)"
		INCLUDE_DIRS                 # Дополнительные include
		RESOURCE_MODULES             # Зависимые модули ресурсов
		RESOURCE_PACKAGES            # Пакеты дополнительных ресурсов, объявленные при помощи RegisterResource
		REMOTE_RESOURCE_PACKAGES     # Пакеты дополнительных удалённых ресурсов, объявленные при помощи RegisterRemoteResource
		MODULES                      # имена подключаемых 3rdparty-модулей. E.g. boost, OpenGLSupport, boost::filesystem, WinLicense, opencv::imgcodecs. Модули могут иметь зависимости - например, boost::filesystem еще и подключает хедеры.
		QT_USE                       # используемые модули Qt.
		MACOS_FRAMEWORKS             # Фреймворки MacOS. E.g. IOKit
		LINK_TARGETS                 # зависимые цели для компоновки. E.g. CoreInt.
		LINK_LIBRARIES               # дополнительные библиотеки для компоновк. E.g. [ WIN32 d3d.lib ]
		PLUGINS                      # Зависимости для сборки, которые будут использованы при фиксапе
		DEPENDENCIES                 # Указываются другие цели, сборка которых должна происходить раньше этой
		QT_PLUGINS			         # Плагины QT. Например: QWindowsAudio для импортируемой либы Qt5::QWindowsAudioPlugin (См. QT_DIR/lib/cmake/Qt5Multimedia)
		QT_QML_MODULES               # QML плагины
		QRC                          # Дополнительные *.qrc файлы с ресурсами, будут подключены и влинкованы в этот модуль
		FORMS                        # Дополнительные *.ui файлы
		SOURCES                      # Дополнительные файлы с исходниками.
		LINK_FLAGS                   # Флаги компоновки
		MAC_XIB                      # Маковские  ui-файлы
		CONFIGS                      # Здесь указываются файлы конфигурирующие данное приложение (пресеты, ...)
		WIN_RC                       # Дополнительные *.rc файлы с виндовыми ресурсами, будут подключены и влинкованы в этот модуль
		REPO_DEPENDENCIES            # Зависимые репозитории (их NAME), которые будут добавлены в качестве INCLUDE_DIRS.
	)
	ParseArgumentsWithConditions(ARG "${__options}" "${__one_val_required}" "${__one_val_optional}" "${__multi_val}" ${ARGN})

	message( STATUS "Add target: ${ARG_NAME}")

	list_exists_item(EXCLUDED_PLUGINS ${ARG_NAME} isExcluded)
	if (isExcluded)
		return()
	endif()

	initProject()

	set(publicIncludes)
	if (ARG_EXPORT_INCLUDE)
		append(publicIncludes ${ARG_SOURCE_DIR})
	endif()

	if (repoName AND ${repoName}_SRC_DIR)
		append(publicIncludes ${${repoName}_SRC_DIR})
		append(ARG_INCLUDE_DIRS ${${repoName}_SRC_DIR})
	endif()

	if( ARG_QT_USE)
		# Принудительно выставляем опцию QT, если используется QT_USE
		set( ARG_QT TRUE )
	endif()

	if( ARG_QT )
		# Подключаем qt.cmake
		append( ARG_MODULES qt )
		append_unique( ARG_QT_USE Core )
		ExpandQtDependencies(ARG_QT_USE)
		if (WIN32)
			foreach (qtmodule ${ARG_QT_USE})
				append( ARG_MODULES qtdll::${qtmodule})
			endforeach()
		endif()
		if(APPLE)
			append_unique(ARG_QT_PLUGINS QCocoaIntegration)
		elseif(WIN32)
			append_unique(ARG_QT_PLUGINS QWindowsIntegration)
		endif()
		append_unique(QT_PLUGINS_FILES ${ARG_QT_PLUGINS})
		set( QT_PLUGINS_FILES ${QT_PLUGINS_FILES} CACHE INTERNAL "" FORCE)
	endif()

	append( EXTRA_INCLUDE_DIRECTORIES "${ARG_SOURCE_DIR}" ${ARG_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})

	PrepareTargetSources(ALL_SOURCES EXTRA_INCLUDE_DIRECTORIES)

	if(ARG_COMPILER_OPTIONS)
		# Дополнительные флаги компиляции
		foreach(instruction ${ARG_COMPILER_OPTIONS})
			set(exist_${instruction} true)
			if (NOT MSVC)
				CHECK_CXX_COMPILER_FLAG(${instruction} exist_${instruction})
			endif()
			if(${exist_${instruction}})
				append( EXTRA_COMPILE_FLAGS ${instruction} )
			endif()
		endforeach()
	endif()

	set( CreateTarget )
	if(${ARG_TYPE} STREQUAL static_lib)
		set( TargetType STATIC )
		set( CreateTarget "library" )
	elseif(${ARG_TYPE} STREQUAL shared_lib)
		set( TargetType SHARED )
		set( CreateTarget "library" )
		string(TOUPPER "${ARG_NAME}" ProjNameUpperCase)
		CreateExportLibFile( ${ARG_NAME} ${ProjNameUpperCase}_API ${ARG_NAME}Lib.h )
	elseif((${ARG_TYPE} STREQUAL app
				OR ${ARG_TYPE} STREQUAL app_bundle )
			AND WIN32)
		set( TargetType WIN32 )
		set( CreateTarget "executable" )
	elseif(${ARG_TYPE} STREQUAL app_console)
		set( TargetType "" )
		set( CreateTarget "executable" )
	elseif(${ARG_TYPE} STREQUAL app_bundle AND APPLE)
		set( TargetType MACOSX_BUNDLE )
		set( CreateTarget "executable" )
	elseif(${ARG_TYPE} STREQUAL app AND APPLE)
		set( TargetType "" )
		set( CreateTarget "executable" )
	elseif((${ARG_TYPE} STREQUAL app
			OR ${ARG_TYPE} STREQUAL app_bundle ) AND LINUX)
		set( TargetType "" )
		set( CreateTarget "executable" )
	elseif(${ARG_TYPE} STREQUAL header_only)
		# TODO: Interface targets are not presetnted in IDE
		#add_library(${ARG_NAME} INTERFACE)
		#target_sources(${ARG_NAME} INTERFACE ${ALL_SOURCES})

		set( TargetType STATIC )
		set( CreateTarget "library" )
		configure_file(${SCRIPT_HELPERS_DIR}/HeaderOnlyLibraryStub.cpp.in ${ARG_NAME}_stub.cpp)
		append(ALL_SOURCES ${ARG_NAME}_stub.cpp)
	else()
		message( FATAL_ERROR "Unknown target TYPE: ${ARG_TYPE}" )
	endif()

	set ( PROJECT_GROUP_PREFIX )
	if (NOT DEFINED OBJLIB_SUFFIX)
		set ( OBJLIB_SUFFIX _ )
	endif()

	if(CreateTarget STREQUAL "library")
		add_library( ${ARG_NAME}
			${TargetType}
			${ALL_SOURCES}
		)
	elseif(CreateTarget STREQUAL "executable")
		add_executable( ${ARG_NAME}
			${TargetType}
			${ALL_SOURCES}
		)
	endif()

	# подключаем сторонние библиотеки
	foreach (prop
			EXTRA_LINK_LIBRARIES_DEBUG
			EXTRA_LINK_LIBRARIES_RELEASE
			EXTRA_LINK_FLAGS
			EXTRA_INCLUDE_DIRECTORIES
			EXTRA_LINK_DIRECTORIES
			EXTRA_PRIVATE_DEFINITIONS
			EXTRA_COMPILE_FLAGS
			EXTRA_WORKSPACE_FILES
			EXTRA_PLUGINS
			EXTRA_PLUGINS_DEBUG
			EXTRA_PLUGINS_RELEASE
			EXTRA_DEBUGINFO_DEBUG
			EXTRA_DEBUGINFO_RELEASE
			EXTRA_MACOS_FRAMEWORKS
			)
		foreach(includeModule ${ARG_MODULES})
			if (NOT TARGET ${includeModule})
				message(FATAL_ERROR "${ARG_NAME} has an invalid MODULE: ${includeModule}")
			endif()
			get_target_property(value ${includeModule} ${prop})
			if (value)
				list(APPEND ${prop} ${value})
			endif()
		endforeach()
		if (${prop})
			# On Linux we have a hack -- we duplicate library dependeny info, because
			# our configuration system does not care about.
			# For saving this info we reverse library list before removing duplicates,
			# because dependent library must be to the left of it's dependencies.
			list(REVERSE ${prop})
			list(REMOVE_DUPLICATES ${prop})
			list(REVERSE ${prop})
		endif()
	endforeach()

	if (ARG_QT_USE)
		foreach (qt_item ${ARG_QT_USE})
			if (qt_item STREQUAL "OpenGLExtensions")
				continue()
			endif()
			target_link_libraries(${ARG_NAME} LINK_PRIVATE Qt5::${qt_item})
		endforeach()
	endif()

	set(output_name ${ARG_NAME})
	if(ARG_OUTPUT_NAME)
		set(output_name ${ARG_OUTPUT_NAME})
	endif()
	setTargetOutput( ${ARG_NAME} ${ARG_TYPE} ${output_name} )

	if (TargetType STREQUAL "MACOSX_BUNDLE")
		GenerateQtConf("${output_name}.app/Contents/Resources")
		GenerateNoFixupFile("${output_name}.app")
	endif()

	if (APPLE AND ${ARG_TYPE} STREQUAL app_console)
		GenerateQtConf("")
	endif()

	if( ARG_LINK_TARGETS )
		append(EXTRA_LINK_LIBRARIES ${ARG_LINK_TARGETS})
	endif()
	if (ARG_LINK_LIBRARIES)
		append(EXTRA_LINK_LIBRARIES ${ARG_LINK_LIBRARIES})
	endif()
	if( APPLE )
		foreach(framework Foundation ${ARG_MACOS_FRAMEWORKS} ${EXTRA_MACOS_FRAMEWORKS})
			string(TOUPPER ${framework} upper_framework)
			if(NOT FRAMEWORK_${upper_framework})
				message(SEND_ERROR "Not found framework '${framework}'")
			endif()
			append(EXTRA_LINK_LIBRARIES ${FRAMEWORK_${upper_framework}})
		endforeach()
	endif()

	# Директивы препроцессора
	if( ARG_COMPILE_DEFINITIONS )
		append( EXTRA_PRIVATE_DEFINITIONS ${ARG_COMPILE_DEFINITIONS} )
	endif( )

	foreach(FDEBUG_LIBRARY ${EXTRA_LINK_LIBRARIES_DEBUG})
		append( EXTRA_LINK_LIBRARIES debug ${FDEBUG_LIBRARY})
	endforeach()
	foreach(FRELEASE_LIBRARY ${EXTRA_LINK_LIBRARIES_RELEASE})
		append( EXTRA_LINK_LIBRARIES optimized ${FRELEASE_LIBRARY})
	endforeach()

	if( ARG_LINK_FLAGS )
		append( EXTRA_LINK_FLAGS ${ARG_LINK_FLAGS} )
	endif()

	postTarget( ${ARG_NAME} )

	# Зависимости
	if( ARG_DEPENDENCIES )
		add_dependencies( ${ARG_NAME} ${ARG_DEPENDENCIES})
	endif()
	# уберем из списка плагинов занесенные в черный список в продуктовой конфигурации
	if( ARG_PLUGINS )
		if (EXCLUDED_PLUGINS)
			list(REMOVE_ITEM ARG_PLUGINS ${EXCLUDED_PLUGINS})
		endif()
		if (ARG_PLUGINS)
			add_dependencies( ${ARG_NAME} ${ARG_PLUGINS})
		endif()
	endif()

	# для фиксапа в Install.
	set_target_properties(${ARG_NAME} PROPERTIES
		EXTRA_LINK_LIBRARIES               "${EXTRA_LINK_LIBRARIES}"
		LINK_TARGETS                       "${ARG_LINK_TARGETS}"
		WORKSPACE_FILES                    "${EXTRA_WORKSPACE_FILES}"
		PLUGINS_RELEASE                    "${EXTRA_PLUGINS};${EXTRA_PLUGINS_RELEASE}"
		PLUGINS_DEBUG                      "${EXTRA_PLUGINS};${EXTRA_PLUGINS_DEBUG}"
		EXTRA_PLUGINS                      "${ARG_PLUGINS}"
		QT_USE                             "${ARG_QT_USE}"
		QT_PLUGINS                         "${ARG_QT_PLUGINS}"
		QT_QML_MODULES                     "${ARG_QT_QML_MODULES}"
		EXTRA_FRAMEWORKS                   "${EXTRA_MACOS_FRAMEWORKS}"
	)
	if (WIN32)
		set_target_properties(${ARG_NAME} PROPERTIES
			WIN_NEED_PROTECTION	     "${ARG_NEED_PROTECTION}"
		)
	endif()

	if (publicIncludes)
		target_include_directories(${ARG_NAME} PUBLIC ${publicIncludes})
	endif()

	if( ARG_PROJECT_GROUP )
		setProjectFolder( ${ARG_NAME} ${ARG_PROJECT_GROUP} )
	endif()

	if(ARG_ENTITLEMENTS)
		set(ENTITLEMENTS_PATH "${ARG_ENTITLEMENTS}")
		if(NOT IS_ABSOLUTE ${ENTITLEMENTS_PATH})
			set(ENTITLEMENTS_PATH "${ARG_SOURCE_DIR}/${ENTITLEMENTS_PATH}")
		endif()
		set(ENTITLEMENTS_${ARG_NAME} ${ENTITLEMENTS_PATH} CACHE STRING "" FORCE)
	endif()

	if ( WIN32 AND ARG_WIN_APP_ICON)
		get_target_property(TARGET_OUTPUT_NAME ${ARG_NAME} OUTPUT_NAME)

		#Проверяются переменные, которые должны быть в глобальной области видимости после
		# GrabAllMapVariables(${PRODUCT_CONFIG}Build SEPARATE_COMMA)
		# GrabAllMapVariables(${PRODUCT_CONFIG}CommonSettings PRODUCT_NAME)
		CheckRequiredVariables(
			ORGANIZATION_NAME                 # название организации
			PRODUCT_NAME                      # название продукта
			PRODUCT_NAME_ABOUT                # название продукта в меню "О программе"
			PRODUCT_NAME_FILE_DESCRIPTION     # название продукта отображаемое пользователю в Windows (напр. в меню "Открыть с помощью")
			PRODUCT_NAME_VERSIONED            # навзание продукта с номером версии включительно
			PRODUCT_VERSION_MAJOR             # старшая версия продукта
			PRODUCT_VERSION_MINOR             # младшая версия продукта
			SCRIPT_HELPERS_DIR                # путь расположения директории .../helpers
			COPYRIGHT_YEAR                    # год регистрации авторского права
			ARG_WIN_APP_ICON                  # путь расположения иконки инсталляции
			TARGET_OUTPUT_NAME                # наименование исполняемого файла
		)

		set(APP_ICON ${ARG_WIN_APP_ICON})
		set(rc_file_name "win_resources.rc")
		configure_file(${SCRIPT_HELPERS_DIR}/win/${rc_file_name}.in ${ARG_NAME}/${rc_file_name})
	endif()

	set(appTypesEntitlements app_bundle app)
	if(APPLE AND MAC_APP_STORE AND ARG_TYPE IN_LIST appTypesEntitlements)
		# Appstore требует указания entitlements
		CheckNonEmptyVariable(ARG_ENTITLEMENTS "${ARG_NAME}: appstore requirement for (${appTypesEntitlements}) target types")
	endif()

	if(APPLE AND ${ARG_TYPE} STREQUAL app_bundle)
		# Configure bundle

		CheckNonEmptyVariable(ARG_BUNDLE_INFO_PLIST "${ARG_NAME}: app_bundle requirement")
		CheckNonEmptyVariable(ARG_BUNDLE_ICONS      "${ARG_NAME}: app_bundle requirement")

		set(BUNDLE_INFO_PLIST_PATH "${ARG_BUNDLE_INFO_PLIST}")
		set(BUNDLE_ICON_FILE_PATH  "${ARG_BUNDLE_ICONS}")

		if(NOT IS_ABSOLUTE ${BUNDLE_INFO_PLIST_PATH})
			set(BUNDLE_INFO_PLIST_PATH "${ARG_SOURCE_DIR}/${BUNDLE_INFO_PLIST_PATH}")
		endif()

		if(NOT IS_ABSOLUTE ${BUNDLE_ICON_FILE_PATH})
			set(BUNDLE_ICON_FILE_PATH "${ARG_SOURCE_DIR}/${BUNDLE_ICON_FILE_PATH}")
		endif()

		set_target_properties(${ARG_NAME} PROPERTIES
			MACOSX_BUNDLE_INFO_PLIST ${BUNDLE_INFO_PLIST_PATH}
			MACOSX_BUNDLE_ICON_FILE ${BUNDLE_ICON_FILE_PATH}
		)
	endif()

	if (ARG_CONFIGS)
		unset(${ARG_NAME}_INSTALL_CONFIGS CACHE)

		foreach(configFile ${ARG_CONFIGS})
			file(REMOVE ${BIN_DIR}/${configFile})

			set(configFileFullPath)
			find_file(configFileFullPath ${configFile} PATHS ${CONFIG_SEARCH_DIR} NO_DEFAULT_PATH NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)
			append(${ARG_NAME}_INSTALL_CONFIGS ${configFileFullPath})
			unset(configFileFullPath CACHE)

		endforeach(configFile ${ARG_CONFIGS})

		file(COPY ${${ARG_NAME}_INSTALL_CONFIGS} DESTINATION ${BIN_DIR})

		set(${ARG_NAME}_INSTALL_CONFIGS "${${ARG_NAME}_INSTALL_CONFIGS}" CACHE STRING "" FORCE)
	endif()

	set(allWorkspaceFiles ${EXTRA_WORKSPACE_FILES} ${EXTRA_PLUGINS} )
	if (CMAKE_BUILD_TYPE STREQUAL "Release")
		append(allWorkspaceFiles ${EXTRA_PLUGINS_RELEASE} ${EXTRA_DEBUGINFO_RELEASE})
	elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
		append(allWorkspaceFiles ${EXTRA_PLUGINS_DEBUG} ${EXTRA_DEBUGINFO_DEBUG})
	else()
		append(allWorkspaceFiles ${EXTRA_PLUGINS_RELEASE} ${EXTRA_PLUGINS_DEBUG} ${EXTRA_DEBUGINFO_RELEASE} ${EXTRA_DEBUGINFO_DEBUG})
	endif()
	if (APPLE)
		set(allWorkspaceFiles ${EXTRA_WORKSPACE_FILES}) # нам не нужно помещать библиотеки в bin до фиксапа.
	endif()
	if (allWorkspaceFiles)
		append_unique(PREPARE_WORKSPACE_FILES ${allWorkspaceFiles})
	endif()
	set( PREPARE_WORKSPACE_FILES ${PREPARE_WORKSPACE_FILES} CACHE INTERNAL "" FORCE)
endfunction()
