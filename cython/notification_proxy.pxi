cdef extern from "libimobiledevice/notification_proxy.h":
    cdef struct np_client_private:
        pass
    ctypedef np_client_private *np_client_t
    ctypedef enum NPClientErrorEnum:
        NP_E_SUCCESS = 0
        NP_E_INVALID_ARG = -1
        NP_E_PLIST_ERROR = -2
        NP_E_CONN_FAILED = -3
        NP_E_UNKNOWN_ERROR = -256
    GQuark np_client_error_quark()

    ctypedef void (*np_notify_cb_t) (const_char_ptr notification, void *userdata)

    np_client_t np_client_new(idevice_t device, uint16_t port, GError **error)
    void np_client_free(np_client_t client, GError **error)
    void np_post_notification(np_client_t client, char *notification, GError **error)
    void np_observe_notification(np_client_t client, char *notification, GError **error)
    void np_observe_notifications(np_client_t client, char **notification_spec, GError **error)
    void np_set_notify_callback(np_client_t client, np_notify_cb_t notify_cb, void *userdata, GError **error)

cdef void np_notify_cb(const_char_ptr notification, void *py_callback):
    (<object>py_callback)(notification)

NotificationProxyClientError = pyglib_register_exception_for_domain("imobiledevice.NotificationProxyClientError",
    np_client_error_quark())

cdef class NotificationProxyClient(PropertyListService):
    __service_name__ = "com.apple.mobile.notification_proxy"
    cdef np_client_t _c_client

    def __cinit__(self, iDevice device not None, int port, *args, **kwargs):
        cdef GError *err = NULL
        self._c_client = np_client_new(device._c_dev, port, &err)
        handle_error(err)

    def __dealloc__(self):
        cdef GError *err = NULL
        if self._c_client is not NULL:
            np_client_free(self._c_client, &err)
            handle_error(err)

    cpdef set_notify_callback(self, object callback):
        cdef GError *err = NULL
        np_set_notify_callback(self._c_client, np_notify_cb, <void*>callback, &err)
        handle_error(err)

    cpdef observe_notification(self, bytes notification):
        cdef GError *err = NULL
        np_observe_notification(self._c_client, notification, &err)
        handle_error(err)

    cpdef post_notification(self, bytes notification):
        cdef GError *err = NULL
        np_post_notification(self._c_client, notification, &err)
        handle_error(err)
