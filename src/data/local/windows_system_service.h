#pragma once
#include "features/system/local_system_service.h"
class QSettings;

class WindowsSystemService final : public LocalSystemService {
    Q_OBJECT
  public:
    explicit WindowsSystemService(QSettings &settings, QObject *parent = nullptr);
    LocalComputerInfo information() const override {
        return m_info;
    }
    void refresh() override;
    void setDisplayName(const QString &name) override;

  private:
    QSettings &m_settings;
    LocalComputerInfo m_info;
};
