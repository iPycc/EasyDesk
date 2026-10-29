QT += core gui widgets svg
QT -= network
CONFIG += c++17 release
CONFIG -= debug debug_and_release
TEMPLATE = app
TARGET = EasyDesk
DESTDIR = $$PWD/build/bin
INCLUDEPATH += $$PWD/src
SOURCES += \
    src/app/application_context.cpp \
    src/app/main.cpp \
    src/data/local/local_session_service.cpp \
    src/data/local/settings_device_service.cpp \
    src/data/local/windows_system_service.cpp \
    src/ui/components/device_list.cpp \
    src/ui/components/selection_indicator.cpp \
    src/ui/controllers/quick_launch_controller.cpp \
    src/core/device.cpp \
    src/data/mock/mock_transfer_service.cpp \
    src/features/devices/device_service.cpp \
    src/ui/components/controls.cpp \
    src/ui/components/desktop_view.cpp \
    src/ui/components/icons.cpp \
    src/ui/components/menus.cpp \
    src/ui/controllers/connection_controller.cpp \
    src/ui/controllers/device_controller.cpp \
    src/ui/controllers/session_controller.cpp \
    src/ui/controllers/settings_controller.cpp \
    src/ui/dialogs/about_dialog.cpp \
    src/ui/dialogs/device_dialogs.cpp \
    src/ui/dialogs/transfer_dialog.cpp \
    src/ui/pages/connect_page.cpp \
    src/ui/pages/device_list_page.cpp \
    src/ui/pages/device_page.cpp \
    src/ui/pages/settings_page.cpp \
    src/ui/theme/theme.cpp \
    src/ui/windows/dialog.cpp \
    src/ui/windows/main_navigation.cpp \
    src/ui/windows/main_window.cpp \
    src/ui/windows/session_window.cpp \
    src/ui/windows/window_chrome.cpp
HEADERS += \
    src/app/application_context.h \
    src/core/device.h \
    src/core/local_computer.h \
    src/features/system/local_system_service.h \
    src/data/local/local_session_service.h \
    src/data/local/settings_device_service.h \
    src/data/local/windows_system_service.h \
    src/ui/components/device_list.h \
    src/ui/components/selection_indicator.h \
    src/core/session.h \
    src/core/transfer.h \
    src/data/mock/mock_transfer_service.h \
    src/features/devices/device_service.h \
    src/features/sessions/session_service.h \
    src/features/transfers/transfer_service.h \
    src/ui/components/controls.h \
    src/ui/components/desktop_view.h \
    src/ui/components/icons.h \
    src/ui/components/layout_helpers.h \
    src/ui/theme/theme.h \
    src/ui/ui.h \
    src/ui/windows/dialog.h \
    src/ui/windows/main_window.h \
    src/ui/windows/session_window.h \
    src/ui/windows/window_chrome.h
RESOURCES += assets/resources.qrc
win32 {
    LIBS += -ldwmapi -luser32 -ladvapi32 -liphlpapi
    QMAKE_CXXFLAGS += /utf-8
    RC_FILE = assets/app.rc
    RC_INCLUDEPATH += $$PWD/assets
}
equals(QT_VERSION, 5.15.12) {
} else {
    error("EasyDesk requires exactly Qt 5.15.12. Active Qt: $$QT_VERSION")
}
