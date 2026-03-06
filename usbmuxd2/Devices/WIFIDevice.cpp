//
//  WIFIDevice.cpp
//  usbmuxd2
//

#include <libgeneral/macros.h>

#ifdef HAVE_LIBIMOBILEDEVICE

#include "WIFIDevice.hpp"
#include "WIFIConnectionSession.hpp"
#include "../Muxer.hpp"
#include "../sysconf/sysconf.hpp"

#ifdef HAVE_WIFI_AVAHI
#   include "../Manager/WIFIDeviceManager-avahi.hpp"
#elif HAVE_WIFI_MDNS
#   include "../Manager/WIFIDeviceManager-mDNS.hpp"
#endif

#include <assert.h>
#include <string.h>

#if defined(HAVE_WIFI_AVAHI) || defined(HAVE_WIFI_MDNS)

WIFIDevice::WIFIDevice(Muxer *mux, WIFIDeviceManager *parent, std::string uuid, std::vector<std::string> ipaddr, std::string serviceName, uint32_t interfaceIndex)
: Device(mux,Device::MUXCONN_WIFI), _parent(parent), _ipaddr(ipaddr), _serviceName(serviceName), _interfaceIndex(interfaceIndex), _session(nullptr), _rediscoverOnDestruct(true)
{
    strncpy(_serial, uuid.c_str(), sizeof(_serial));
}

WIFIDevice::~WIFIDevice() {
    debug("deleting device %s",_serial);
    stopSession(true);
    {
        std::unique_lock<std::mutex> ul(_parent->_childrenLck);
        _parent->_children.erase(this);
        _parent->_childrenEvent.notifyAll();
        _parent = NULL;
    }
}

bool WIFIDevice::isPairingDevice() const noexcept{
    return strncmp(_serial, "WIFIPAIR", sizeof("WIFIPAIR")-1) == 0;
}

void WIFIDevice::ensureSession(){
    if (isPairingDevice()) {
        return;
    }
    std::lock_guard<std::mutex> lg(_sessionLck);
    if (!_session) {
        _session = std::make_shared<WIFIConnectionSession>(_selfref.lock());
    }
    _session->start();
}

void WIFIDevice::stopSession(bool joinThread) noexcept{
    std::shared_ptr<WIFIConnectionSession> session;
    {
        std::lock_guard<std::mutex> lg(_sessionLck);
        session = _session;
        _session.reset();
    }
    if (session) {
        session->stop(joinThread);
    }
}

void WIFIDevice::updateDiscoveryInfo(std::vector<std::string> ipaddr, std::string serviceName, uint32_t interfaceIndex){
    std::lock_guard<std::mutex> lg(_sessionLck);
    _ipaddr = std::move(ipaddr);
    _serviceName = std::move(serviceName);
    _interfaceIndex = interfaceIndex;
}

void WIFIDevice::setRediscoverOnDestruct(bool enabled) noexcept{
    _rediscoverOnDestruct = enabled;
}

void WIFIDevice::kill() noexcept{
    warning("[Killing] WIFIDevice serial=%s service=%s",_serial,_serviceName.c_str());
    std::shared_ptr<WIFIDevice> selfref = _selfref.lock();
    _parent->_reapDevices.post(selfref);
}

void WIFIDevice::deconstruct() noexcept{
    warning("[Deconstructing] WIFIDevice serial=%s service=%s ip_count=%zu",_serial,_serviceName.c_str(),_ipaddr.size());
    std::shared_ptr<WIFIDevice> selfref = _selfref.lock();
    stopSession(true);
    _mux->delete_device(selfref);
#if defined(HAVE_WIFI_AVAHI) || defined(HAVE_WIFI_MDNS)
    if (_rediscoverOnDestruct && _parent) {
        _parent->request_device_rediscovery(_serial, _serviceName.c_str());
    }
#endif
}

void WIFIDevice::startLoop(){
    ensureSession();
}

void WIFIDevice::start_connect(uint16_t dport, std::shared_ptr<Client> cli){
    reterror("Legacy connection proxying is currently not implemented");
}

#endif //defined(HAVE_WIFI_AVAHI) || defined(HAVE_WIFI_MDNS)
#endif //HAVE_LIBIMOBILEDEVICE
