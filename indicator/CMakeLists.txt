add_definitions(-DTRANSLATION_DOMAIN=\"kdeconnect-indicator\")

set(indicator_SRCS
    main.cpp
    deviceindicator.cpp
)

include(ECMAddAppIcon)
ecm_add_app_icon(indicator_SRCS ICONS
    ../icon/16-apps-kdeconnect.png
    ../icon/22-apps-kdeconnect.png
    ../icon/32-apps-kdeconnect.png
    ../icon/48-apps-kdeconnect.png
    ../icon/64-apps-kdeconnect.png
    ../icon/128-apps-kdeconnect.png
    ../icon/256-apps-kdeconnect.png
)

if (WIN32)
    list(APPEND indicator_SRCS indicatorhelper_win.cpp)
elseif (APPLE)
    list(APPEND indicator_SRCS indicatorhelper_mac.cpp)
else ()
    list(APPEND indicator_SRCS indicatorhelper.cpp)
endif()

add_executable(kdeconnect-indicator ${indicator_SRCS})
target_include_directories(kdeconnect-indicator PUBLIC ${CMAKE_BINARY_DIR})
target_link_libraries(kdeconnect-indicator Qt5::Widgets KF5::CoreAddons KF5::I18n KF5::Notifications KF5::DBusAddons KF5::KCMUtils kdeconnectinterfaces kdeconnectcore)
if (WIN32)
    add_compile_definitions(QSYSTRAY)
endif()

if (APPLE)
# Apple app package
set_target_properties(kdeconnect-indicator PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
set_target_properties(kdeconnect-indicator PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "KDE Connect"
    MACOSX_BUNDLE_LONG_VERSION_STRING "${KDECONNECT_VERSION_STRING}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${KDECONNECT_VERSION_MAJOR}.${KDECONNECT_VERSION_MINOR}.${KDECONNECT_VERSION_PATCH}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.kde.kdeconnect")
# Indicator launchd configuration
configure_file(org.kde.kdeconnect-indicator.plist.in ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kdeconnect-indicator.plist)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kdeconnect-indicator.plist DESTINATION ${AUTOSTART_INSTALL_DIR})
endif()

install(TARGETS kdeconnect-indicator ${INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS org.kde.kdeconnect.nonplasma.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
