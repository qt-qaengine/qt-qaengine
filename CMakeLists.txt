# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Copyright (c) 2026 Andrei Kozhevnikov <coderusinbox@gmail.com>

cmake_minimum_required(VERSION 3.16)

# Load local CMake definitions. Before the project() command strictly!
find_file(LOCAL_DEFS local_defs.cmake "${CMAKE_CURRENT_BINARY_DIR}" NO_DEFAULT_PATH)
if (LOCAL_DEFS)
    include("${CMAKE_CURRENT_BINARY_DIR}/local_defs.cmake")
    message(STATUS "File ${CMAKE_CURRENT_BINARY_DIR}/local_defs.cmake loaded.")
elseif (CALLED_FROM_CBE)
    message(FATAL_ERROR "Can't find file ${CMAKE_CURRENT_BINARY_DIR}/local_defs.cmake!")
endif ()

project(qtqaengine VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Enable widgets backend. Disabled by default, same as in qtqaengine.pro.
option(QA_ENGINE_WITH_WIDGETS "Build the QWidgets engine platform" OFF)

set(LIBS "")
set(LIBS_DEFS "")
set(LIBS_INCLUDES "")
# Qt components this build actually uses, re-exported to qtqaengineConfig.cmake.
set(QA_ENGINE_QT_COMPONENTS "")
set(${PROJECT_NAME}_HEADERS "")
set(${PROJECT_NAME}_SOURCES "")

if(UNIX AND NOT APPLE)
    # Linux
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    # Temporary use MO_PLATFORM_MOBILE
    set(MO_PATFORM_MOBILE ON)
endif()

# Prefer Qt6, fall back to Qt5.
find_package(QT ${CURRENT_QT_VERSION} NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
set(QT Qt${QT_VERSION_MAJOR})
message(STATUS "Building against ${QT} ${QT_VERSION}")

# Qt6 ships private modules as separate components/targets, Qt5 exposes them only as
# ${QT}<Module>_PRIVATE_INCLUDE_DIRS variables. qa_use_qt_module() hides the difference:
# qa_use_qt_module(Quick PRIVATE) links the module and makes its private headers usable.
macro(qa_use_qt_module module)
    cmake_parse_arguments(_qa "PRIVATE" "" "" ${ARGN})

    if (QT_VERSION_MAJOR GREATER_EQUAL 6)
        set(_qa_components ${module})
        if (_qa_PRIVATE)
            list(APPEND _qa_components ${module}Private)
        endif()
        find_package(${QT} ${CURRENT_QT_VERSION} REQUIRED COMPONENTS ${_qa_components})

        list(APPEND QA_ENGINE_QT_COMPONENTS ${_qa_components})
        list(APPEND LIBS ${QT}::${module})
        if (_qa_PRIVATE)
            list(APPEND LIBS ${QT}::${module}Private)
        endif()
    else()
        find_package(${QT} ${CURRENT_QT_VERSION} REQUIRED COMPONENTS ${module})

        list(APPEND QA_ENGINE_QT_COMPONENTS ${module})
        list(APPEND LIBS ${QT}::${module})
        list(APPEND LIBS_INCLUDES ${${QT}${module}_INCLUDE_DIRS})
        list(APPEND LIBS_DEFS ${${QT}${module}_DEFINITIONS})
        if (_qa_PRIVATE)
            list(APPEND LIBS_INCLUDES ${${QT}${module}_PRIVATE_INCLUDE_DIRS})
        endif()
    endif()
endmacro()

# Core / Gui / Network. Private modules are needed for qhooks_p.h, qmetaobject_p.h,
# qmetatype_p.h, qvariantanimation_p.h, qeventpoint_p.h and qwindow_p.h.
qa_use_qt_module(Core PRIVATE)
qa_use_qt_module(Gui PRIVATE)
qa_use_qt_module(Network)

find_package(${QT} ${CURRENT_QT_VERSION} QUIET COMPONENTS Quick)
set(QA_ENGINE_HAS_QUICK ${${QT}Quick_FOUND})
if (QA_ENGINE_HAS_QUICK)
    list(APPEND LIBS_DEFS MO_USE_QUICK)

    qa_use_qt_module(Qml)
    qa_use_qt_module(Quick PRIVATE)

    # QQuickPopup support is compiled in for Qt 5.10 and newer only,
    # see the QT_VERSION_CHECK(5, 10, 0) guards in src/QuickEnginePlatform.cpp.
    if (QT_VERSION VERSION_GREATER_EQUAL 5.10)
        qa_use_qt_module(QuickControls2 PRIVATE)

        # QuickEnginePlatform.cpp includes <QtQuickTemplates2/private/qquickpopup_p.h>.
        # QuickTemplates2 is an internal module: Qt6 exports it as a component, Qt5 does
        # not always ship a CMake config for it, so fall back to the on-disk header layout
        # (<qt>/include/QtQuickTemplates2/<version>/QtQuickTemplates2/private/*.h).
        find_package(${QT} ${CURRENT_QT_VERSION} QUIET COMPONENTS QuickTemplates2)
        if (${QT}QuickTemplates2_FOUND)
            qa_use_qt_module(QuickTemplates2 PRIVATE)
        else()
            list(GET ${QT}Core_INCLUDE_DIRS 0 _qa_qtcore_include)
            get_filename_component(_qa_qt_include "${_qa_qtcore_include}" DIRECTORY)
            list(APPEND LIBS_INCLUDES
                "${_qa_qt_include}/QtQuickTemplates2"
                "${_qa_qt_include}/QtQuickTemplates2/${QT_VERSION}"
                "${_qa_qt_include}/QtQuickTemplates2/${QT_VERSION}/QtQuickTemplates2"
            )
            message(STATUS "${QT}QuickTemplates2 CMake config not found, using headers from ${_qa_qt_include}/QtQuickTemplates2")
        endif()
    endif()

    list(APPEND ${PROJECT_NAME}_HEADERS include/qtqaengine/QuickEnginePlatform.h)
    list(APPEND ${PROJECT_NAME}_SOURCES src/QuickEnginePlatform.cpp)

    if (QT_VERSION_MAJOR GREATER_EQUAL 6)
        qt6_add_resources(QRC_RESOURCES qml/qtqaengine.qrc)
    else()
        qt5_add_resources(QRC_RESOURCES qml/qtqaengine.qrc)
    endif()
endif()

if (QA_ENGINE_WITH_WIDGETS)
    list(APPEND LIBS_DEFS MO_USE_QWIDGETS)
    qa_use_qt_module(Widgets PRIVATE)

    list(APPEND ${PROJECT_NAME}_HEADERS include/qtqaengine/WidgetsEnginePlatform.h)
    list(APPEND ${PROJECT_NAME}_SOURCES src/WidgetsEnginePlatform.cpp)
endif()

if (QT_VERSION_MAJOR LESS 6)
    # workaround for ios cmake bug with wrong paths in _PRIVATE_INCLUDE_DIRS
    # removing Versions/5/ from its paths
    foreach(di ${LIBS_INCLUDES})
        if(NOT IS_DIRECTORY ${di})
            string(REPLACE "Versions/5/" "" do "${di}")
            if(NOT "${di}" STREQUAL "${do}" AND IS_DIRECTORY ${do})
                list(APPEND LIBS_INCLUDES ${do})
            endif()
        endif()
    endforeach()
endif()

list(APPEND
    ${PROJECT_NAME}_HEADERS
    include/qtqaengine/GenericEnginePlatform.h
    include/qtqaengine/ITransportServer.h
    include/qtqaengine/IEnginePlatform.h
    include/qtqaengine/QtQaEngine.h
    include/qtqaengine/QAKeyMouseEngine.h
    include/qtqaengine/TCPSocketClient.h
    include/qtqaengine/ITransportClient.h
    include/qtqaengine/QAEngineSocketClient.h
    include/qtqaengine/QAPendingEvent.h
    include/qtqaengine/TCPSocketServer.h
)

list(APPEND
    ${PROJECT_NAME}_SOURCES
    src/GenericEnginePlatform.cpp
    src/QtQaEngine.cpp
    src/QAPendingEvent.cpp
    src/IEnginePlatform.cpp
    src/QAEngineSocketClient.cpp
    src/ITransportClient.cpp
    src/TCPSocketClient.cpp
    src/ITransportServer.cpp
    src/QAKeyMouseEngine.cpp
    src/TCPSocketServer.cpp
    src/loader.cpp
)

# > Generated moc_*.cpp and *.moc files are placed in the build directory
# so it is convenient to set the CMAKE_INCLUDE_CURRENT_DIR variable.
# @see https://cmake.org/cmake/help/latest/manual/cmake-qt.7.html#automoc

add_library (${PROJECT_NAME} SHARED
    ${QA_LIB_CONFIG}
    ${QRC_RESOURCES}
    ${${PROJECT_NAME}_SOURCES}
    ${${PROJECT_NAME}_HEADERS}
)

# Qt6 requires C++17, Qt5 builds keep the C++11 baseline of qtqaengine.pro.
if (QT_VERSION_MAJOR GREATER_EQUAL 6)
    set(QA_ENGINE_CXX_STANDARD 17)
else()
    set(QA_ENGINE_CXX_STANDARD 11)
endif()

set_target_properties (${PROJECT_NAME} PROPERTIES
    CXX_STANDARD ${QA_ENGINE_CXX_STANDARD}
    CXX_STANDARD_REQUIRED YES
    AUTOMOC ON
    EXPORT_NAME QtQaEngineLib
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        ${LIBS_DEFS}
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ${LIBS}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${LIBS_INCLUDES}
    PUBLIC
        $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}>
)

if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti -fexceptions -fPIC")
endif()

if (MO_OS_IOS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fembed-bitcode")
endif()

# Installation
include(CMakePackageConfigHelpers)

set(TARGETS_EXPORT_NAME "${PROJECT_NAME}Targets")
set(TARGETS_EXPORT_FILE "${TARGETS_EXPORT_NAME}.cmake")
set(PROJECT_CONFIG "${PROJECT_NAME}Config.cmake")
set(PROJECT_CONFIG_IN "${PROJECT_CONFIG}.in")
set(NAMESPACE "QtQaEngine::")
set(REQUIRED_QT_VERSION "${QT_VERSION}")
list(REMOVE_DUPLICATES QA_ENGINE_QT_COMPONENTS)
string(REPLACE ";" " " QA_ENGINE_QT_COMPONENTS "${QA_ENGINE_QT_COMPONENTS}")

if (ANDROID)
    set(CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}/")
else()
    set(CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/")
endif()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(${CMAKE_CURRENT_LIST_DIR}/cmake/${PROJECT_CONFIG_IN}
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_CONFIG}
    INSTALL_DESTINATION ${CONFIG_INSTALL_DIR}
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_CONFIG}
    DESTINATION ${CONFIG_INSTALL_DIR}
)

install(TARGETS ${PROJECT_NAME} EXPORT ${TARGETS_EXPORT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT ${TARGETS_EXPORT_NAME}
    FILE ${TARGETS_EXPORT_FILE}
    NAMESPACE ${NAMESPACE}
    DESTINATION ${CONFIG_INSTALL_DIR}
)
