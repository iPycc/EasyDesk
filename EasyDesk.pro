QT += core gui widgets svg
QT -= network
CONFIG += c++17 release
CONFIG -= debug debug_and_release
TEMPLATE = app
TARGET = EasyDesk
DESTDIR = $$PWD/build/bin
INCLUDEPATH += \
    $$PWD/src/client/application \
    $$PWD/src/client/domain \
    $$PWD/src/client/infrastructure \
    $$PWD/src/client/platform/windows \
    $$PWD/src/client/presentation
SOURCES += \
    src/client/application/app/application_context.cpp \
    src/client/application/app/main.cpp \
    src/client/domain/core/device.cpp \
    src/client/domain/features/devices/device_service.cpp \
    src/client/infrastructure/data/local/local_session_service.cpp \
    src/client/infrastructure/data/local/local_transfer_service.cpp \
    src/client/infrastructure/data/local/settings_device_service.cpp \
    src/client/platform/windows/data/local/windows_system_service.cpp \
    src/client/presentation/ui/components/controls.cpp \
    src/client/presentation/ui/components/desktop_view.cpp \
    src/client/presentation/ui/components/device_list.cpp \
    src/client/presentation/ui/components/icons.cpp \
    src/client/presentation/ui/components/menus.cpp \
    src/client/presentation/ui/components/selection_indicator.cpp \
    src/client/presentation/ui/controllers/connection_controller.cpp \
    src/client/presentation/ui/controllers/device_controller.cpp \
    src/client/presentation/ui/controllers/session_controller.cpp \
    src/client/presentation/ui/controllers/settings_controller.cpp \
    src/client/presentation/ui/dialogs/about_dialog.cpp \
    src/client/presentation/ui/dialogs/device_dialogs.cpp \
    src/client/presentation/ui/dialogs/transfer_dialog.cpp \
    src/client/presentation/ui/pages/connect_page.cpp \
    src/client/presentation/ui/pages/device_list_page.cpp \
    src/client/presentation/ui/pages/device_page.cpp \
    src/client/presentation/ui/pages/settings_page.cpp \
    src/client/presentation/ui/theme/theme.cpp \
    src/client/presentation/ui/windows/dialog.cpp \
    src/client/presentation/ui/windows/main_navigation.cpp \
    src/client/presentation/ui/windows/main_window.cpp \
    src/client/presentation/ui/windows/session_window.cpp \
    src/client/presentation/ui/windows/window_chrome.cpp
HEADERS += \
    src/client/application/app/application_context.h \
    src/client/domain/core/device.h \
    src/client/domain/core/local_computer.h \
    src/client/domain/core/session.h \
    src/client/domain/core/transfer.h \
    src/client/domain/features/devices/device_service.h \
    src/client/domain/features/sessions/session_service.h \
    src/client/domain/features/system/local_system_service.h \
    src/client/domain/features/transfers/transfer_service.h \
    src/client/infrastructure/data/local/local_session_service.h \
    src/client/infrastructure/data/local/local_transfer_service.h \
    src/client/infrastructure/data/local/settings_device_service.h \
    src/client/platform/windows/data/local/windows_system_service.h \
    src/client/presentation/ui/components/controls.h \
    src/client/presentation/ui/components/desktop_view.h \
    src/client/presentation/ui/components/device_list.h \
    src/client/presentation/ui/components/icons.h \
    src/client/presentation/ui/components/layout_helpers.h \
    src/client/presentation/ui/components/selection_indicator.h \
    src/client/presentation/ui/theme/theme.h \
    src/client/presentation/ui/ui.h \
    src/client/presentation/ui/windows/dialog.h \
    src/client/presentation/ui/windows/main_window.h \
    src/client/presentation/ui/windows/session_window.h \
    src/client/presentation/ui/windows/window_chrome.h
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
