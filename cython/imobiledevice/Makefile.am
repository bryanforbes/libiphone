AM_CPPFLAGS = -I$(top_srcdir)/include

AM_CFLAGS = $(libglib2_CFLAGS) $(libgnutls_CFLAGS) $(libtasn1_CFLAGS) $(libgthread2_CFLAGS) $(LFS_CFLAGS)
AM_LDFLAGS = $(libglib2_LIBS) $(libgnutls_LIBS) $(libtasn1_LIBS) $(libgthread2_LIBS)

if HAVE_CYTHON

BUILT_SOURCES = 			\
	afc.c					\
	base.c					\
	file_relay.c			\
	idevice.c				\
	installation_proxy.c	\
	lockdown.c				\
	mobilebackup.c			\
	mobile_image_mounter.c	\
	mobilesync.c			\
	notification_proxy.c	\
	sbservices.c			\
	screenshotr.c

PXDINCLUDES = std.pxi $(CYTHON_PLIST_INCLUDE_DIR)/plist.pxd

CLEANFILES =                \
        *.pyc               \
        *.pyo               \
		*.c

EXTRA_DIST =				\
	__init__.py				\
	afc.pyx					\
	base.pxd				\
	base.pyx				\
	file_relay.pyx			\
	idevice.pxd				\
	idevice.pyx				\
	installation_proxy.pyx	\
	lockdown.pyx			\
	std.pxi

imobiledevicedir = $(pyexecdir)/imobiledevice
imobiledevice_DATA = __init__.py

afcdir = $(imobiledevicedir)
afc_LTLIBRARIES = afc.la
afc_la_SOURCES = afc.pyx
afc_la_CFLAGS = -I../../include -I../src $(PYTHON_CPPFLAGS) $(AM_CFLAGS)
afc_la_LDFLAGS = -module -avoid-version -L$(libdir) $(PYTHON_LDFLAGS) $(AM_LDFLAGS)
afc_la_LIBADD = ../../src/libimobiledevice.la
afc.c: afc.pyx idevice.pxd base.pxd $(PXDINCLUDES)

basedir = $(imobiledevicedir)
base_LTLIBRARIES = base.la
base_la_SOURCES = base.pyx
base_la_CFLAGS = -I../../include -I../../src $(PYTHON_CPPFLAGS) $(AM_CFLAGS)
base_la_LDFLAGS = -module -avoid-version -L$(libdir) $(PYTHON_LDFLAGS) $(AM_LDFLAGS)
base_la_LIBADD = ../../src/libimobiledevice.la
base.c: base.pyx base.pxd $(PXDINCLUDES)

file_relaydir = $(imobiledevicedir)
file_relay_LTLIBRARIES = file_relay.la
file_relay_la_SOURCES = file_relay.pyx
file_relay_la_CFLAGS = -I../../include -I../src $(PYTHON_CPPFLAGS) $(AM_CFLAGS)
file_relay_la_LDFLAGS = -module -avoid-version -L$(libdir) $(PYTHON_LDFLAGS) $(AM_LDFLAGS)
file_relay_la_LIBADD = ../../src/libimobiledevice.la
file_relay.c: file_relay.pyx idevice.pxd base.pxd $(PXDINCLUDES)

installation_proxydir = $(imobiledevicedir)
installation_proxy_LTLIBRARIES = installation_proxy.la
installation_proxy_la_SOURCES = installation_proxy.pyx
installation_proxy_la_CFLAGS = -I../../include -I../src $(PYTHON_CPPFLAGS) $(AM_CFLAGS)
installation_proxy_la_LDFLAGS = -module -avoid-version -L$(libdir) $(PYTHON_LDFLAGS) $(AM_LDFLAGS)
installation_proxy_la_LIBADD = ../../src/libimobiledevice.la
installation_proxy.c: installation_proxy.pyx idevice.pxd base.pxd $(PXDINCLUDES)

idevicedir = $(imobiledevicedir)
idevice_LTLIBRARIES = idevice.la
idevice_la_SOURCES = idevice.pyx
idevice_la_CFLAGS = -I../../include -I../src $(PYTHON_CPPFLAGS) $(AM_CFLAGS)
idevice_la_LDFLAGS = -module -avoid-version -L$(libdir) $(PYTHON_LDFLAGS) $(AM_LDFLAGS)
idevice_la_LIBADD = ../../src/libimobiledevice.la
idevice.c: idevice.pyx idevice.pxd base.pxd $(PXDINCLUDES)

lockdowndir = $(imobiledevicedir)
lockdown_LTLIBRARIES = lockdown.la
lockdown_la_SOURCES = lockdown.pyx
lockdown_la_CFLAGS = -I../../include -I../src $(PYTHON_CPPFLAGS) $(AM_CFLAGS)
lockdown_la_LDFLAGS = -module -avoid-version -L$(libdir) $(PYTHON_LDFLAGS) $(AM_LDFLAGS)
lockdown_la_LIBADD = ../../src/libimobiledevice.la
lockdown.c: lockdown.pyx idevice.pxd base.pxd $(PXDINCLUDES)

mobilebackupdir = $(imobiledevicedir)
mobilebackup_LTLIBRARIES = mobilebackup.la
mobilebackup_la_SOURCES = mobilebackup.pyx
mobilebackup_la_CFLAGS = -I../../include -I../src $(PYTHON_CPPFLAGS) $(AM_CFLAGS)
mobilebackup_la_LDFLAGS = -module -avoid-version -L$(libdir) $(PYTHON_LDFLAGS) $(AM_LDFLAGS)
mobilebackup_la_LIBADD = ../../src/libimobiledevice.la
mobilebackup.c: mobilebackup.pyx idevice.pxd base.pxd $(PXDINCLUDES)

mobile_image_mounterdir = $(imobiledevicedir)
mobile_image_mounter_LTLIBRARIES = mobile_image_mounter.la
mobile_image_mounter_la_SOURCES = mobile_image_mounter.pyx
mobile_image_mounter_la_CFLAGS = -I../../include -I../src $(PYTHON_CPPFLAGS) $(AM_CFLAGS)
mobile_image_mounter_la_LDFLAGS = -module -avoid-version -L$(libdir) $(PYTHON_LDFLAGS) $(AM_LDFLAGS)
mobile_image_mounter_la_LIBADD = ../../src/libimobiledevice.la
mobile_image_mounter.c: mobile_image_mounter.pyx idevice.pxd base.pxd $(PXDINCLUDES)

mobilesyncdir = $(imobiledevicedir)
mobilesync_LTLIBRARIES = mobilesync.la
mobilesync_la_SOURCES = mobilesync.pyx
mobilesync_la_CFLAGS = -I../../include -I../src $(PYTHON_CPPFLAGS) $(AM_CFLAGS)
mobilesync_la_LDFLAGS = -module -avoid-version -L$(libdir) $(PYTHON_LDFLAGS) $(AM_LDFLAGS)
mobilesync_la_LIBADD = ../../src/libimobiledevice.la
mobilesync.c: mobilesync.pyx idevice.pxd base.pxd $(PXDINCLUDES)

notification_proxydir = $(imobiledevicedir)
notification_proxy_LTLIBRARIES = notification_proxy.la
notification_proxy_la_SOURCES = notification_proxy.pyx
notification_proxy_la_CFLAGS = -I../../include -I../src $(PYTHON_CPPFLAGS) $(AM_CFLAGS)
notification_proxy_la_LDFLAGS = -module -avoid-version -L$(libdir) $(PYTHON_LDFLAGS) $(AM_LDFLAGS)
notification_proxy_la_LIBADD = ../../src/libimobiledevice.la
notification_proxy.c: notification_proxy.pyx idevice.pxd base.pxd $(PXDINCLUDES)

sbservicesdir = $(imobiledevicedir)
sbservices_LTLIBRARIES = sbservices.la
sbservices_la_SOURCES = sbservices.pyx
sbservices_la_CFLAGS = -I../../include -I../src $(PYTHON_CPPFLAGS) $(AM_CFLAGS)
sbservices_la_LDFLAGS = -module -avoid-version -L$(libdir) $(PYTHON_LDFLAGS) $(AM_LDFLAGS)
sbservices_la_LIBADD = ../../src/libimobiledevice.la
sbservices.c: sbservices.pyx idevice.pxd base.pxd $(PXDINCLUDES)

screenshotrdir = $(imobiledevicedir)
screenshotr_LTLIBRARIES = screenshotr.la
screenshotr_la_SOURCES = screenshotr.pyx
screenshotr_la_CFLAGS = -I../../include -I../src $(PYTHON_CPPFLAGS) $(AM_CFLAGS)
screenshotr_la_LDFLAGS = -module -avoid-version -L$(libdir) $(PYTHON_LDFLAGS) $(AM_LDFLAGS)
screenshotr_la_LIBADD = ../../src/libimobiledevice.la
screenshotr.c: screenshotr.pyx idevice.pxd base.pxd $(PXDINCLUDES)

.pyx.c:
	$(CYTHON) -X embedsignature=True -I$(CYTHON_PLIST_INCLUDE_DIR) -I$(top_srcdir)/src -o $@ $<

# imobiledevice_private.c: $(IMOBILEDEVICE_PRIVATE_SOURCES) $(IMOBILEDEVICE_INCLUDES) $(PLIST_INCLUDES)
#	$(CYTHON) $(IMOBILEDEVICE_CPPFLAGS) -I$(top_srcdir)/src -o $@ $<

endif
