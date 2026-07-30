cmake_minimum_required(VERSION 3.19)
project(PicoBuild LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)

qt_standard_project_setup()

qt_add_executable(PicoBuild
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    projectconfigdialog.cpp
    projectconfigdialog.h
    languagemanager.cpp
    languagemanager.h
    resources.qrc
    resources.rc
    Raspberry_Pi.ico
)

set_source_files_properties(resources.rc PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Raspberry_Pi.ico")

qt_add_translations(
    TARGETS PicoBuild
    TS_FILES PicoBuild_zh_CN.ts
)

target_link_libraries(PicoBuild
    PRIVATE
        Qt::Core
        Qt::Widgets
)

add_custom_command(TARGET PicoBuild POST_BUILD
    COMMAND "${QT6_INSTALL_PREFIX}/bin/windeployqt.exe" --no-translations "$<TARGET_FILE:PicoBuild>"
    COMMENT "Deploying Qt DLLs..."
)

install(TARGETS PicoBuild
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET PicoBuild
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})