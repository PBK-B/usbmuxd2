//
//  WIFIConnectionSession.cpp
//  usbmuxd2
//

#include <libgeneral/macros.h>

#ifdef HAVE_LIBIMOBILEDEVICE

#include "WIFIConnectionSession.hpp"
#include "WIFIDevice.hpp"
#include "../Muxer.hpp"

#include <libimobiledevice/heartbeat.h>
#include <plist/plist.h>

#include <thread>

WIFIConnectionSession::WIFIConnectionSession(std::shared_ptr<WIFIDevice> device)
: _device(device)
, _worker()
, _stopRequested(false)
, _started(false)
, _state(State::Idle)
, _idev(nullptr)
, _hbclient(nullptr)
{
}

WIFIConnectionSession::~WIFIConnectionSession(){
    stop(true);
    cleanupConnection();
}

void WIFIConnectionSession::cleanupConnection() noexcept{
    safeFreeCustom(_hbclient, heartbeat_client_free);
    safeFreeCustom(_idev, idevice_free);
}

std::chrono::milliseconds WIFIConnectionSession::nextBackoff(size_t &idx) const noexcept{
    static const std::chrono::milliseconds seq[] = {
        std::chrono::seconds(1),
        std::chrono::seconds(2),
        std::chrono::seconds(4),
        std::chrono::seconds(8),
        std::chrono::seconds(16),
        std::chrono::seconds(30),
    };
    auto delay = seq[idx];
    idx = (idx + 1) % (sizeof(seq)/sizeof(seq[0]));
    return delay;
}

bool WIFIConnectionSession::runAttempt(std::shared_ptr<WIFIDevice> dev) noexcept{
    heartbeat_error_t hret = HEARTBEAT_E_SUCCESS;

    cleanupConnection();

    warning("[WIFIConnectionSession] starting connection attempt serial=%s service=%s", dev->_serial, dev->_serviceName.c_str());
    try {
        assure(!idevice_new_with_options(&_idev, dev->_serial, IDEVICE_LOOKUP_NETWORK));
    } catch (tihmstar::exception &e) {
        warning("[WIFIConnectionSession] idevice lookup failed serial=%s error=%d (%s)", dev->_serial, e.code(), e.what());
        cleanupConnection();
        return false;
    }

    if ((hret = heartbeat_client_start_service(_idev, &_hbclient, "usbmuxd2")) != HEARTBEAT_E_SUCCESS) {
        warning("[WIFIConnectionSession] heartbeat service start failed serial=%s error=%d", dev->_serial, hret);
        cleanupConnection();
        return false;
    }

    warning("[WIFIConnectionSession] heartbeat session established serial=%s", dev->_serial);
    return true;
}

void WIFIConnectionSession::runloop() noexcept{
    size_t backoffIdx = 0;

    try {
    while (!_stopRequested) {
        auto dev = _device.lock();
        if (!dev) {
            break;
        }

        _state = State::Connecting;
        bool connected = runAttempt(dev);
        if (connected) {
            _state = State::Connected;
            plist_t hbrsp = nullptr;
            plist_t hbeat = nullptr;
            assure(hbrsp = plist_new_dict());
            plist_dict_set_item(hbrsp, "Command", plist_new_string("Polo"));
            while (!_stopRequested) {
                heartbeat_error_t hret = heartbeat_receive_with_timeout(_hbclient, &hbeat, 15000);
                if (hret != HEARTBEAT_E_SUCCESS) {
                    warning("[WIFIConnectionSession] heartbeat receive failed serial=%s error=%d", dev->_serial, hret);
                    break;
                }
                hret = heartbeat_send(_hbclient, hbrsp);
                safeFreeCustom(hbeat, plist_free);
                if (hret != HEARTBEAT_E_SUCCESS) {
                    warning("[WIFIConnectionSession] heartbeat send failed serial=%s error=%d", dev->_serial, hret);
                    break;
                }
            }
            safeFreeCustom(hbeat, plist_free);
            safeFreeCustom(hbrsp, plist_free);

            if (_stopRequested) {
                break;
            }

            if (!dev->_mux->allowHeartlessWifi()) {
                warning("[WIFIConnectionSession] removing wifi device after heartbeat failure serial=%s because allowHeartlessWifi=NO", dev->_serial);
                dev->kill();
                break;
            }
        } else if (!dev->_mux->allowHeartlessWifi()) {
            warning("[WIFIConnectionSession] removing wifi device after startup failure serial=%s because allowHeartlessWifi=NO", dev->_serial);
            dev->kill();
            break;
        }

        if (!dev->_mux->retryWifiSession()) {
            warning("[WIFIConnectionSession] session retry disabled serial=%s", dev->_serial);
            break;
        }

        _state = State::Retrying;
        auto delay = nextBackoff(backoffIdx);
        warning("[WIFIConnectionSession] retrying wifi session serial=%s in %lld ms", dev->_serial, (long long)delay.count());
        std::this_thread::sleep_for(delay);
    }
    } catch (tihmstar::exception &e) {
        error("[WIFIConnectionSession] session thread aborted with error=%d (%s)", e.code(), e.what());
    }

    cleanupConnection();
    _state = State::Stopped;
    _started = false;
}

void WIFIConnectionSession::start(){
    if (_worker.joinable()) {
        _worker.join();
    }
    bool expected = false;
    if (!_started.compare_exchange_strong(expected, true)) {
        return;
    }
    _stopRequested = false;
    _worker = std::thread([this]{ runloop(); });
}

void WIFIConnectionSession::stop(bool joinThread) noexcept{
    _stopRequested = true;
    _state = State::Stopping;
    cleanupConnection();
    if (joinThread && _worker.joinable()) {
        _worker.join();
        _started = false;
    }
}

WIFIConnectionSession::State WIFIConnectionSession::state() const noexcept{
    return _state.load();
}

bool WIFIConnectionSession::hasHeartbeat() const noexcept{
    return _hbclient != nullptr;
}

heartbeat_client_t WIFIConnectionSession::heartbeatClient() const noexcept{
    return _hbclient;
}

#endif //HAVE_LIBIMOBILEDEVICE
