#pragma once
#include <QWidget>
#include <QMap>
#include "features/devices/device_service.h"
#include "features/sessions/session_service.h"
#include "features/transfers/transfer_service.h"
#include "features/system/local_system_service.h"
#include "ui/components/device_list.h"
#include "ui/components/selection_indicator.h"
#include "ui/ui.h"
class QStackedWidget;
class QLabel;
class QPushButton;
class QLineEdit;
class QComboBox;
class QTableView;
class QTabWidget;
class QSettings;
class QDialog;
class DesktopView;
class QVBoxLayout;

class MainWindow : public QWidget {
    Q_OBJECT
  public:
    MainWindow(DeviceService &devices, SessionService &sessions, TransferService &transfers,
               LocalSystemService &local, QSettings &settings, QWidget *parent = nullptr);
    void navigate(const QString &route);

  protected:
    bool nativeEvent(const QByteArray &, void *message, long *result) override;
    void resizeEvent(QResizeEvent *) override;
    void closeEvent(QCloseEvent *) override;

  private:
    DeviceService &m_devices;
    SessionService &m_sessions;
    TransferService &m_transfers;
    LocalSystemService &m_local;
    Ui::SelectionIndicator *m_navIndicator = nullptr;
    QWidget *m_nav = nullptr;
    QVBoxLayout *m_navLayout = nullptr;
    QStackedWidget *m_pages = nullptr;
    Ui::Toast *m_status = nullptr;
    QLabel *m_deviceTitle = nullptr, *m_onlineBadge = nullptr;
    QPushButton *m_favorite = nullptr, *m_connect = nullptr;
    DesktopView *m_preview = nullptr;
    QTabWidget *m_tabs = nullptr;
    QLineEdit *m_remoteId = nullptr, *m_remoteCode = nullptr, *m_search = nullptr;
    QComboBox *m_filter = nullptr;
    Ui::DeviceList *m_deviceList = nullptr;
    QSettings *m_settings = nullptr;
    Ui::SessionWindow *m_session = nullptr;
    QPointer<Ui::Dialog> m_transferDialog;
    QMap<QString, QPushButton *> m_routes;
    QString m_currentId, m_route = "device";
    bool m_favoritesOnly = false;
    bool m_myExpanded = true, m_remoteExpanded = true;
    void buildTitleBar(QVBoxLayout *layout);
    QWidget *buildSidebar();
    QWidget *buildDevicePage();
    QWidget *buildListPage();
    QWidget *buildStartPage();
    QWidget *buildSettingsPage();
    void rebuildNav();
    void updateNavIndicator(bool animate = true);
    void showDeviceDetails(const QString &id);
    void showDevice(const QString &id);
    void refreshDevice();
    void updateFilter();
    void connectRemote();
    void watchDeviceOperation(quint64 requestId, const QString &success,
                              std::function<void()> onSuccess = {});
    void toggleFavorite(const QString &id);
    void addDevice();
    void renameDevice();
    void removeDevice();
    void startSession(const QString &id, const QString &assistanceCode = QString());
    void showIncomingSessionRequest(const IncomingSessionRequest &request);
    void showPeerIdentityPrompt(const PeerIdentityPrompt &prompt);
    void openTransfers();
    void notify(const QString &text);
    void showAbout();
    void applySettings();
    void loadSettings();
};
