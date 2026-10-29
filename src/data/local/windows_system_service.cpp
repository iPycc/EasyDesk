#include "data/local/windows_system_service.h"
#include <QSettings>
#include <QSysInfo>
#ifdef Q_OS_WIN
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <vector>
#endif

namespace {
#ifdef Q_OS_WIN
QString registryText(const wchar_t *key, const wchar_t *name) {
    DWORD size = 0;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, key, name, RRF_RT_REG_SZ, nullptr, nullptr, &size) !=
            ERROR_SUCCESS ||
        size < sizeof(wchar_t))
        return {};
    std::vector<wchar_t> value(size / sizeof(wchar_t) + 1, 0);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, key, name, RRF_RT_REG_SZ, nullptr, value.data(), &size) !=
        ERROR_SUCCESS)
        return {};
    return QString::fromWCharArray(value.data()).trimmed();
}
QStringList adapterAddresses() {
    ULONG size = 16 * 1024;
    std::vector<unsigned char> buffer(size);
    ULONG result = ERROR_BUFFER_OVERFLOW;
    for (int attempt = 0; attempt < 3 && result == ERROR_BUFFER_OVERFLOW; ++attempt) {
        buffer.resize(size);
        result = GetAdaptersAddresses(
            AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr,
            reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data()), &size);
    }
    QStringList values;
    if (result != NO_ERROR)
        return values;
    for (auto *adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data()); adapter;
         adapter = adapter->Next) {
        if (adapter->PhysicalAddressLength != 6 ||
            (adapter->IfType != IF_TYPE_ETHERNET_CSMACD && adapter->IfType != IF_TYPE_IEEE80211))
            continue;
        QStringList address;
        for (ULONG index = 0; index < adapter->PhysicalAddressLength; ++index)
            address << QString("%1").arg(adapter->PhysicalAddress[index], 2, 16, QLatin1Char('0'));
        values << QString::fromWCharArray(adapter->FriendlyName) + "  ·  " + address.join(':').toUpper();
    }
    return values;
}
#endif
} // namespace
WindowsSystemService::WindowsSystemService(QSettings &settings, QObject *parent)
    : LocalSystemService(parent), m_settings(settings) {
    refresh();
}
void WindowsSystemService::refresh() {
    LocalComputerInfo info;
    info.computerName = QSysInfo::machineHostName();
    info.operatingSystem = QSysInfo::prettyProductName();
#ifdef Q_OS_WIN
    wchar_t name[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD count = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameW(name, &count))
        info.computerName = QString::fromWCharArray(name, count);
    info.boardManufacturer = registryText(L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BaseBoardManufacturer");
    info.boardProduct = registryText(L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BaseBoardProduct");
    info.processor =
        registryText(L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString");
    OSVERSIONINFOW version = {};
    version.dwOSVersionInfoSize = sizeof(version);
    using RtlGetVersionFn = LONG(WINAPI *)(OSVERSIONINFOW *);
    auto query =
        reinterpret_cast<RtlGetVersionFn>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    if (query && query(&version) == 0 && version.dwMajorVersion == 10 && version.dwBuildNumber >= 22000)
        info.operatingSystem = "Windows 11";
    const auto displayVersion =
        registryText(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"DisplayVersion");
    if (!displayVersion.isEmpty())
        info.operatingSystem += "  " + displayVersion;
    info.networkAdapters = adapterAddresses();
#endif
    info.displayName = m_settings.value("localDisplayName", info.computerName).toString();
    if (info.displayName.trimmed().isEmpty())
        info.displayName = info.computerName;
    m_info = info;
    emit changed();
}
void WindowsSystemService::setDisplayName(const QString &name) {
    const QString value = name.trimmed().left(40);
    if (value.isEmpty())
        return;
    m_settings.setValue("localDisplayName", value);
    m_settings.sync();
    m_info.displayName = value;
    emit changed();
}
