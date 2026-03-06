AM_CFLAGS = -I$(top_srcdir)/usbmuxd2 $(GLOBAL_CFLAGS) $(libplist_CFLAGS) $(libusb_CFLAGS) $(libimobildevice_CFLAGS) $(libgeneral_CFLAGS)
AM_CXXFLAGS = $(GLOBAL_CXXFLAGS) $(libplist_CXXFLAGS) $(wifi_backend_CFLAGS)
AM_LDFLAGS = $(libplist_LIBS) $(libusb_LIBS) $(libimobiledevice_LIBS) $(wifi_backend_LIBS) $(libpthread_LIBS) $(libgeneral_LIBS)


sbin_PROGRAMS = usbmuxd

usbmuxd_CFLAGS = $(AM_CFLAGS)
usbmuxd_CXXFLAGS = $(AM_CXXFLAGS) $(AM_CFLAGS)
usbmuxd_LDFLAGS = $(AM_LDFLAGS)
usbmuxd_SOURCES = main.cpp \
			log.c \
			Client.cpp \
			Muxer.cpp \
			TCP.cpp \
			sysconf/sysconf.cpp \
			sysconf/preflight.cpp \
			Devices/Device.cpp \
			Devices/USBDevice.cpp \
			Devices/USBDevice_receiver.cpp \
			Devices/WIFIDevice.cpp \
			Manager/USBDeviceManager.cpp \
			Manager/ClientManager.cpp \
			Manager/DeviceManager.cpp

if USE_WIFI_AVAHI
usbmuxd_SOURCES += Manager/WIFIDeviceManager-avahi.cpp
endif

if USE_WIFI_MDNS
usbmuxd_SOURCES += Manager/WIFIDeviceManager-mDNS.cpp
endif
