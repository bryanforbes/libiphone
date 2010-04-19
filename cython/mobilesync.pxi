cdef extern from "libimobiledevice/mobilesync.h":
    cdef struct mobilesync_client_private:
        pass
    ctypedef mobilesync_client_private *mobilesync_client_t

    ctypedef enum MobileSyncErrorEnum:
        MOBILESYNC_E_SUCCESS = 0
        MOBILESYNC_E_INVALID_ARG = -1
        MOBILESYNC_E_PLIST_ERROR = -2
        MOBILESYNC_E_MUX_ERROR = -3
        MOBILESYNC_E_BAD_VERSION = -4
        MOBILESYNC_E_UNKNOWN_ERROR = -256

    GQuark mobilesync_client_error_quark()
    mobilesync_client_t mobilesync_client_new(idevice_t device, uint16_t port, GError **error)
    void mobilesync_client_free(mobilesync_client_t client, GError **error)
    plist.plist_t mobilesync_receive(mobilesync_client_t client, GError **error)
    void mobilesync_send(mobilesync_client_t client, plist.plist_t plist, GError **error)

MobileSyncClientError = pyglib_register_exception_for_domain("imobiledevice.MobileSyncClientError",
    mobilesync_client_error_quark())

cdef class MobileSyncClient(DeviceLinkService):
    __service_name__ = "com.apple.mobilesync"
    cdef mobilesync_client_t _c_client

    def __cinit__(self, iDevice device not None, int port, *args, **kwargs):
        cdef:
            GError *err = NULL
        self._c_client = mobilesync_client_new(device._c_dev, port, &err)
        handle_error(err)
    
    def __dealloc__(self):
        cdef GError *err = NULL
        if self._c_client is not NULL:
            mobilesync_client_free(self._c_client, &err)
            handle_error(err)
    
    cdef inline _send(self, plist.plist_t node, GError **error):
        mobilesync_send(self._c_client, node, error)

    cdef inline plist.plist_t _receive(self, GError **error):
        return mobilesync_receive(self._c_client, error)
