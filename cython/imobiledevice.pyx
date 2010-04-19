pyglib_init()

cdef extern from "libimobiledevice/libimobiledevice.h":
    ctypedef enum iDeviceErrorEnum:
        IDEVICE_E_SUCCESS = 0
        IDEVICE_E_INVALID_ARG = -1
        IDEVICE_E_UNKNOWN_ERROR = -2
        IDEVICE_E_NO_DEVICE = -3
        IDEVICE_E_NOT_ENOUGH_DATA = -4
        IDEVICE_E_BAD_HEADER = -5
        IDEVICE_E_SSL_ERROR = -6
    GQuark       idevice_error_quark()

    void idevice_set_debug_level(int level)
    ctypedef void (*idevice_event_cb_t) (const_idevice_event_t event, void *user_data)
    void idevice_event_subscribe(idevice_event_cb_t callback, void *user_data, GError **error)
    void idevice_event_unsubscribe(GError **error)

    void idevice_get_device_list(char ***devices, int *count, GError **error)
    void idevice_device_list_free(char **devices)

    idevice_t idevice_new(char *uuid, GError **error)
    void idevice_free(idevice_t device)

    idevice_connection_t idevice_connect(idevice_t device, uint16_t port, GError **error)
    void idevice_disconnect(idevice_connection_t connection, GError **error)

    uint32_t idevice_connection_send(idevice_connection_t connection, char *data, uint32_t len, GError **error)
    void idevice_connection_receive_timeout(idevice_connection_t connection, char *data, uint32_t len, uint32_t *recv_bytes, unsigned int timeout, GError **error)
    void idevice_connection_receive(idevice_connection_t connection, char *data, uint32_t len, uint32_t *recv_bytes, GError **error)

    uint32_t idevice_get_handle(idevice_t device, GError **error)
    char* idevice_get_uuid(idevice_t device)

iDeviceError = pyglib_register_exception_for_domain("imobiledevice.iDeviceError", idevice_error_quark())

cdef int handle_error(GError *error) except -1:
    if pyglib_error_check(&error):
        return -1
    return 0

def set_debug_level(int level):
    idevice_set_debug_level(level)

cdef class iDeviceEvent:
    def __init__(self, *args, **kwargs):
        raise TypeError("iDeviceEvent cannot be instantiated")

    def __str__(self):
        return 'iDeviceEvent: %s (%s)' % (self.event == IDEVICE_DEVICE_ADD and 'Add' or 'Remove', self.uuid)

    property event:
        def __get__(self):
            return self._c_event.event
    property uuid:
        def __get__(self):
            return self._c_event.uuid
    property conn_type:
        def __get__(self):
            return self._c_event.conn_type

cdef void idevice_event_cb(const_idevice_event_t c_event, void *user_data) with gil:
    cdef iDeviceEvent event = iDeviceEvent.__new__(iDeviceEvent)
    event._c_event = c_event
    (<object>user_data)(event)

def event_subscribe(object callback):
    cdef GError *err = NULL
    idevice_event_subscribe(idevice_event_cb, <void*>callback, &err)
    handle_error(err)

def event_unsubscribe():
    cdef GError *err = NULL
    idevice_event_unsubscribe(&err)
    handle_error(err)

def get_device_list():
    cdef:
        char** devices
        int count
        list result
        bytes device
        GError *err = NULL

    idevice_get_device_list(&devices, &count, &err)
    try:
        handle_error(err)

        result = []
        for i from 0 <= i < count:
            device = devices[i]
            result.append(device)

        return result
    finally:
        if devices != NULL:
            idevice_device_list_free(devices)

cdef class iDeviceConnection(object):
    def __init__(self, *args, **kwargs):
        raise TypeError("iDeviceConnection cannot be instantiated.  Please use iDevice.connect()")

    cpdef disconnect(self):
        cdef GError *error = NULL
        idevice_disconnect(self._c_connection, &error)
        handle_error(error)

cdef class iDevice(object):
    def __cinit__(self, uuid=None, *args, **kwargs):
        cdef:
            char* c_uuid = NULL
            GError *err = NULL
        if uuid is not None:
            c_uuid = uuid
        self._c_dev = idevice_new(c_uuid, &err)
        handle_error(err)

    def __dealloc__(self):
        if self._c_dev is not NULL:
            idevice_free(self._c_dev)

    cpdef iDeviceConnection connect(self, uint16_t port):
        cdef:
            GError *err = NULL
            idevice_connection_t c_conn = NULL
            iDeviceConnection conn

        c_conn = idevice_connect(self._c_dev, port, &err)
        handle_error(err)

        conn = iDeviceConnection.__new__(iDeviceConnection)
        conn._c_connection = c_conn

        return conn

    property uuid:
        def __get__(self):
            cdef:
                char* uuid
            uuid = idevice_get_uuid(self._c_dev)
            return uuid
    property handle:
        def __get__(self):
            cdef:
                uint32_t handle = 0
                GError *error = NULL
            handle = idevice_get_handle(self._c_dev, &error)
            handle_error(error)
            return handle

cdef extern from *:
    ctypedef char* const_char_ptr "const char*"

cimport stdlib

cdef class BaseService(object):
    __service_name__ = None

cdef extern from "property_list_service.h":
    GQuark property_list_service_error_quark()

PropertyListServiceError = pyglib_register_exception_for_domain(
    "imobiledevice.PropertyListServiceError", property_list_service_error_quark())

cdef class PropertyListService(BaseService):
    cpdef send(self, plist.Node node):
        cdef GError *err = NULL
        self._send(node._c_node, &err)
        handle_error(err)

    cpdef object receive(self):
        cdef:
            plist.plist_t c_node = NULL
            GError *err = NULL
        c_node = self._receive(&err)
        try:
            handle_error(err)
        except Exception, e:
            if c_node != NULL:
                plist.plist_free(c_node)
            raise

        return plist.plist_t_to_node(c_node)

    cdef inline _send(self, plist.plist_t node, GError **error):
        raise NotImplementedError("send is not implemented")

    cdef inline plist.plist_t _receive(self, GError **error):
        raise NotImplementedError("receive is not implemented")

cdef extern from "device_link_service.h":
    GQuark device_link_service_error_quark()

DeviceLinkServiceError = pyglib_register_exception_for_domain(
    "imobiledevice.DeviceLinkServiceError", device_link_service_error_quark())

cdef class DeviceLinkService(PropertyListService):
    pass

include "lockdown.pxi"
include "mobilesync.pxi"
include "notification_proxy.pxi"
include "sbservices.pxi"
include "mobilebackup.pxi"
include "afc.pxi"
include "file_relay.pxi"
include "screenshotr.pxi"
include "installation_proxy.pxi"
include "mobile_image_mounter.pxi"
