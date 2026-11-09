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
    $$PWD/src/client/presentation \
    $$PWD/src/media/bridge/include
SOURCES += \
    src/client/application/app/application_context.cpp \
    src/client/application/app/main.cpp \
    src/client/application/sessions/negotiation/ice/local_ice_candidate_buffer.cpp \
    src/client/application/sessions/negotiation/ice/session_ice_description_binding.cpp \
    src/client/application/sessions/negotiation/authorization/session_authorization_deadline.cpp \
    src/client/application/sessions/negotiation/control/session_ready_exchange.cpp \
    src/client/application/sessions/negotiation/control/session_capture_geometry_codec.cpp \
    src/client/application/sessions/negotiation/ice/session_ice_signal_receiver.cpp \
    src/client/application/sessions/negotiation/ice/session_ice_signal_sender.cpp \
    src/client/application/sessions/negotiation/media/session_media_event_pump.cpp \
    src/client/application/sessions/negotiation/media/session_video_monitor.cpp \
    src/client/application/sessions/negotiation/session_negotiation_coordinator.cpp \
    src/client/application/sessions/negotiation/session_negotiation_context.cpp \
    src/client/application/sessions/negotiation/transport/session_transport_state.cpp \
    src/client/application/sessions/negotiation/turn/session_turn_binding.cpp \
    src/client/application/sessions/input/session_input_lease_host.cpp \
    src/client/application/sessions/input/session_input_lease_client.cpp \
    src/client/application/sessions/input/session_input_coordinator.cpp \
    src/client/application/sessions/input/session_input_transport.cpp \
    src/client/application/sessions/files/session_file_transport.cpp \
    src/client/application/sessions/session_invitation_coordinator.cpp \
    src/client/application/sessions/session_request_coordinator.cpp \
    src/client/application/sessions/termination/session_termination_receiver.cpp \
    src/client/application/sessions/termination/session_termination_sender.cpp \
    src/client/domain/core/device.cpp \
    src/client/domain/features/devices/device_service.cpp \
    src/client/domain/features/sessions/model/session_access.cpp \
    src/client/domain/features/sessions/model/session_state.cpp \
    src/client/domain/features/transfers/transfer_manifest.cpp \
    src/client/infrastructure/data/local/local_transfer_service.cpp \
    src/client/infrastructure/data/local/settings_device_service.cpp \
    src/client/infrastructure/data/settings/identity/device_profile_store.cpp \
    src/client/infrastructure/network/signaling/client/signaling_client.cpp \
    src/client/infrastructure/network/input/protocol/wire/protobuf_wire.cpp \
    src/client/infrastructure/network/input/protocol/input_codec.cpp \
    src/client/infrastructure/network/files/protocol/transfer_offer_codec.cpp \
    src/client/infrastructure/network/input/protocol/lease/input_lease_codec.cpp \
    src/client/infrastructure/network/input/protocol/lease/input_enable_codec.cpp \
    src/client/infrastructure/network/signaling/protocol/bootstrap/bootstrap_message_codec.cpp \
    src/client/infrastructure/network/signaling/protocol/cbor/deterministic_cbor_reader.cpp \
    src/client/infrastructure/network/signaling/protocol/cbor/deterministic_cbor_writer.cpp \
    src/client/infrastructure/network/signaling/protocol/encoding/base64url.cpp \
    src/client/infrastructure/network/signaling/protocol/registration/registration_codec.cpp \
    src/client/infrastructure/network/signaling/protocol/session/session_accept_codec.cpp \
    src/client/infrastructure/network/signaling/protocol/session/session_accept_decoder.cpp \
    src/client/infrastructure/network/signaling/protocol/session/session_capability.cpp \
    src/client/infrastructure/network/signaling/protocol/session/description/dtls_fingerprint.cpp \
    src/client/infrastructure/network/signaling/protocol/session/description/session_description_codec.cpp \
    src/client/infrastructure/network/signaling/protocol/session/description/session_description_decoder.cpp \
    src/client/infrastructure/network/signaling/protocol/session/ice/session_ice_codec.cpp \
    src/client/infrastructure/network/signaling/protocol/session/ice/session_ice_decoder.cpp \
    src/client/infrastructure/network/signaling/protocol/session/session_decision_codec.cpp \
    src/client/infrastructure/network/signaling/protocol/session/session_decision_decoder.cpp \
    src/client/infrastructure/network/signaling/protocol/session/session_delivery_codec.cpp \
    src/client/infrastructure/network/signaling/protocol/session/session_request_codec.cpp \
    src/client/infrastructure/network/signaling/protocol/session/session_request_decoder.cpp \
    src/client/infrastructure/network/signaling/protocol/session/termination/session_end_codec.cpp \
    src/client/infrastructure/network/signaling/protocol/session/termination/session_end_decoder.cpp \
    src/client/infrastructure/network/signaling/protocol/turn/turn_credentials_decoder.cpp \
    src/client/infrastructure/network/signaling/service/signaling_session_service.cpp \
    src/client/platform/windows/data/local/windows_system_service.cpp \
    src/client/platform/windows/input/input_lease.cpp \
    src/client/platform/windows/input/send_input_executor.cpp \
    src/client/platform/windows/input/windows_input_controller.cpp \
    src/client/platform/windows/input/coordinate_mapping.cpp \
    src/client/platform/windows/input/input_ownership.cpp \
    src/client/platform/windows/identity/windows_identity_store.cpp \
    src/client/platform/windows/network/winhttp/winhttp_websocket_connection.cpp \
    src/client/platform/windows/network/winhttp/winhttp_websocket_transport.cpp \
    src/client/platform/windows/security/secure_random.cpp \
    src/client/platform/windows/media/capture/dxgi_desktop_capture.cpp \
    src/client/platform/windows/media/capture/stream/desktop_capture_stream.cpp \
    src/client/platform/windows/media/webrtc/media_runtime_state.cpp \
    src/client/platform/windows/media/webrtc/media_runtime_library.cpp \
    src/client/platform/windows/media/webrtc/media_session.cpp \
    src/client/presentation/ui/components/controls.cpp \
    src/client/presentation/ui/components/desktop_view.cpp \
    src/client/presentation/ui/input/video_input_mapping.cpp \
    src/client/presentation/ui/input/desktop_input_capture.cpp \
    src/client/presentation/ui/components/device_list.cpp \
    src/client/presentation/ui/components/icons.cpp \
    src/client/presentation/ui/components/menus.cpp \
    src/client/presentation/ui/components/selection_indicator.cpp \
    src/client/presentation/ui/controllers/connection_controller.cpp \
    src/client/presentation/ui/controllers/device_controller.cpp \
    src/client/presentation/ui/controllers/session_controller.cpp \
    src/client/presentation/ui/controllers/session_invitation_controller.cpp \
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
    src/client/application/sessions/negotiation/ice/local_ice_candidate_buffer.h \
    src/client/application/sessions/negotiation/ice/session_ice_description_binding.h \
    src/client/application/sessions/negotiation/authorization/session_authorization_deadline.h \
    src/client/application/sessions/negotiation/control/session_ready_exchange.h \
    src/client/application/sessions/negotiation/control/session_capture_geometry_codec.h \
    src/client/application/sessions/negotiation/ice/session_ice_signal_receiver.h \
    src/client/application/sessions/negotiation/ice/session_ice_signal_sender.h \
    src/client/application/sessions/negotiation/media/session_media_event_pump.h \
    src/client/application/sessions/negotiation/media/session_video_monitor.h \
    src/client/application/sessions/negotiation/session_negotiation_coordinator.h \
    src/client/application/sessions/negotiation/session_negotiation_context.h \
    src/client/application/sessions/negotiation/transport/session_transport_state.h \
    src/client/application/sessions/negotiation/turn/session_turn_binding.h \
    src/client/application/sessions/input/session_input_lease_host.h \
    src/client/application/sessions/input/session_input_lease_client.h \
    src/client/application/sessions/input/session_input_coordinator.h \
    src/client/application/sessions/input/session_input_transport.h \
    src/client/application/sessions/files/session_file_transport.h \
    src/client/application/sessions/session_invitation_coordinator.h \
    src/client/application/sessions/session_request_coordinator.h \
    src/client/application/sessions/termination/session_termination_receiver.h \
    src/client/application/sessions/termination/session_termination_sender.h \
    src/client/domain/core/device.h \
    src/client/domain/core/local_computer.h \
    src/client/domain/core/session.h \
    src/client/domain/core/transfer.h \
    src/client/domain/features/devices/device_service.h \
    src/client/domain/features/sessions/input/input_command.h \
    src/client/domain/features/sessions/session_service.h \
    src/client/domain/features/sessions/model/session_access.h \
    src/client/domain/features/sessions/model/session_state.h \
    src/client/domain/features/system/local_system_service.h \
    src/client/domain/features/transfers/transfer_service.h \
    src/client/domain/features/transfers/transfer_manifest.h \
    src/client/infrastructure/data/local/local_transfer_service.h \
    src/client/infrastructure/data/local/settings_device_service.h \
    src/client/infrastructure/data/settings/identity/device_profile_store.h \
    src/client/infrastructure/network/signaling/client/signaling_client.h \
    src/client/infrastructure/network/input/protocol/wire/protobuf_wire.h \
    src/client/infrastructure/network/input/protocol/input_codec.h \
    src/client/infrastructure/network/files/protocol/transfer_offer_codec.h \
    src/client/infrastructure/network/input/protocol/lease/input_lease_codec.h \
    src/client/infrastructure/network/input/protocol/lease/input_enable_codec.h \
    src/client/infrastructure/network/signaling/protocol/bootstrap/bootstrap_message_codec.h \
    src/client/infrastructure/network/signaling/protocol/cbor/deterministic_cbor_reader.h \
    src/client/infrastructure/network/signaling/protocol/cbor/deterministic_cbor_writer.h \
    src/client/infrastructure/network/signaling/protocol/encoding/base64url.h \
    src/client/infrastructure/network/signaling/protocol/registration/registration_codec.h \
    src/client/infrastructure/network/signaling/protocol/session/session_accept_codec.h \
    src/client/infrastructure/network/signaling/protocol/session/session_accept_decoder.h \
    src/client/infrastructure/network/signaling/protocol/session/session_capability.h \
    src/client/infrastructure/network/signaling/protocol/session/description/dtls_fingerprint.h \
    src/client/infrastructure/network/signaling/protocol/session/description/session_description_codec.h \
    src/client/infrastructure/network/signaling/protocol/session/description/session_description_decoder.h \
    src/client/infrastructure/network/signaling/protocol/session/ice/session_ice_codec.h \
    src/client/infrastructure/network/signaling/protocol/session/ice/session_ice_decoder.h \
    src/client/infrastructure/network/signaling/protocol/session/session_decision_codec.h \
    src/client/infrastructure/network/signaling/protocol/session/session_decision_decoder.h \
    src/client/infrastructure/network/signaling/protocol/session/session_delivery_codec.h \
    src/client/infrastructure/network/signaling/protocol/session/session_request_codec.h \
    src/client/infrastructure/network/signaling/protocol/session/session_request_decoder.h \
    src/client/infrastructure/network/signaling/protocol/session/termination/session_end_codec.h \
    src/client/infrastructure/network/signaling/protocol/session/termination/session_end_decoder.h \
    src/client/infrastructure/network/signaling/protocol/turn/turn_credentials_decoder.h \
    src/client/infrastructure/network/signaling/service/signaling_session_service.h \
    src/client/platform/windows/data/local/windows_system_service.h \
    src/client/platform/windows/input/input_lease.h \
    src/client/platform/windows/input/send_input_executor.h \
    src/client/platform/windows/input/windows_input_controller.h \
    src/client/platform/windows/input/coordinate_mapping.h \
    src/client/platform/windows/input/input_ownership.h \
    src/client/platform/windows/identity/windows_identity_store.h \
    src/client/platform/windows/network/winhttp/winhttp_websocket_connection.h \
    src/client/platform/windows/network/winhttp/winhttp_websocket_transport.h \
    src/client/platform/windows/security/secure_random.h \
    src/client/platform/windows/media/capture/desktop_capture_frame.h \
    src/client/platform/windows/media/capture/dxgi_desktop_capture.h \
    src/client/platform/windows/media/capture/stream/desktop_capture_stream.h \
    src/client/platform/windows/media/webrtc/media_api.h \
    src/client/platform/windows/media/webrtc/media_runtime_library.h \
    src/client/platform/windows/media/webrtc/media_runtime_state.h \
    src/client/platform/windows/media/webrtc/media_session.h \
    src/media/bridge/include/easydesk_media_abi.h \
    src/client/presentation/ui/components/controls.h \
    src/client/presentation/ui/components/desktop_view.h \
    src/client/presentation/ui/input/video_input_mapping.h \
    src/client/presentation/ui/input/desktop_input_capture.h \
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
    LIBS += -ldwmapi -luser32 -ladvapi32 -liphlpapi -ld3d11 -ldxgi -lbcrypt -lcrypt32 -lwinhttp
    QMAKE_CXXFLAGS += /utf-8
    RC_FILE = assets/app.rc
    RC_INCLUDEPATH += $$PWD/assets
}
equals(QT_VERSION, 5.15.12) {
} else {
    error("EasyDesk requires exactly Qt 5.15.12. Active Qt: $$QT_VERSION")
}
