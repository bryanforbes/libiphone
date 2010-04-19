cdef extern from "libimobiledevice/installation_proxy.h":
    cdef struct instproxy_client_private:
        pass
    ctypedef instproxy_client_private *instproxy_client_t
    ctypedef void (*instproxy_status_cb_t) (const_char_ptr operation, plist.plist_t status, void *user_data)

    ctypedef enum InstProxyClientErrorEnum:
        INSTPROXY_E_SUCCESS = 0
        INSTPROXY_E_INVALID_ARG = -1
        INSTPROXY_E_PLIST_ERROR = -2
        INSTPROXY_E_CONN_FAILED = -3
        INSTPROXY_E_OP_IN_PROGRESS = -4
        INSTPROXY_E_OP_FAILED = -5
        INSTPROXY_E_UNKNOWN_ERROR = -256

    GQuark instproxy_client_error_quark()
    instproxy_client_t instproxy_client_new(idevice_t device, uint16_t port, GError **error)
    void instproxy_client_free(instproxy_client_t client, GError **error)

    plist.plist_t instproxy_browse(instproxy_client_t client, plist.plist_t client_options, GError **error)
    void instproxy_install(instproxy_client_t client, char *pkg_path, plist.plist_t client_options, instproxy_status_cb_t status_cb, void *user_data, GError **error)
    void instproxy_upgrade(instproxy_client_t client, char *pkg_path, plist.plist_t client_options, instproxy_status_cb_t status_cb, void *user_data, GError **error)
    void instproxy_uninstall(instproxy_client_t client, char *appid, plist.plist_t client_options, instproxy_status_cb_t status_cb, void *user_data, GError **error)

    plist.plist_t instproxy_lookup_archives(instproxy_client_t client, plist.plist_t client_options, GError **error)
    void instproxy_archive(instproxy_client_t client, char *appid, plist.plist_t client_options, instproxy_status_cb_t status_cb, void *user_data, GError **error)
    void instproxy_restore(instproxy_client_t client, char *appid, plist.plist_t client_options, instproxy_status_cb_t status_cb, void *user_data, GError **error)
    void instproxy_remove_archive(instproxy_client_t client, char *appid, plist.plist_t client_options, instproxy_status_cb_t status_cb, void *user_data, GError **error)

cdef void instproxy_notify_cb(const_char_ptr operation, plist.plist_t status, void *py_callback) with gil:
    (<object>py_callback)(operation, plist.plist_t_to_node(status, False))

InstallationProxyClientError = pyglib_register_exception_for_domain(
    "imobiledevice.InstallationProxyClientError", instproxy_client_error_quark())

cdef class InstallationProxyClient(PropertyListService):
    __service_name__ = "com.apple.mobile.installation_proxy"
    cdef instproxy_client_t _c_client

    def __cinit__(self, iDevice device not None, int port, *args, **kwargs):
        cdef GError *err = NULL
        self._c_client = instproxy_client_new(device._c_dev, port, &err)
        handle_error(err)

    def __dealloc__(self):
        cdef GError *err = NULL
        if self._c_client is not NULL:
            instproxy_client_free(self._c_client, &err)
            handle_error(err)

    cpdef plist.Node browse(self, object client_options):
        cdef:
            plist.Node options
            plist.plist_t c_options
            plist.plist_t c_result = NULL
            bint do_free = 0
            GError *err = NULL
        if isinstance(client_options, plist.Dict):
            options = client_options
            c_options = options._c_node
        elif isinstance(client_options, dict):
            c_options = plist.native_to_plist_t(client_options)
            do_free = 1
        else:
            raise TypeError("Must pass a plist.Dict or dict to browse")
        c_result = instproxy_browse(self._c_client, c_options, &err)

        try:
            handle_error(err)
        except Exception, e:
            if do_free:
                plist.plist_free(c_options)

        return plist.plist_t_to_node(c_result)

    cpdef install(self, bytes pkg_path, object client_options, object callback=None):
        cdef:
            plist.Node options
            plist.plist_t c_options
            GError *err = NULL
        if isinstance(client_options, plist.Dict):
            options = client_options
            c_options = options._c_node
        elif isinstance(client_options, dict):
            c_options = plist.native_to_plist_t(client_options)
        else:
            raise TypeError("Must pass a plist.Dict or dict to browse")
        if callback is None:
            instproxy_install(self._c_client, pkg_path, options._c_node, NULL, NULL, &err)
        else:
            instproxy_install(self._c_client, pkg_path, options._c_node, instproxy_notify_cb, <void*>callback, &err)
        handle_error(err)

    cpdef upgrade(self, bytes pkg_path, object client_options, object callback=None):
        cdef:
            plist.Node options
            plist.plist_t c_options
            GError *err = NULL
        if isinstance(client_options, plist.Dict):
            options = client_options
            c_options = options._c_node
        elif isinstance(client_options, dict):
            c_options = plist.native_to_plist_t(client_options)
        else:
            raise TypeError("Must pass a plist.Dict or dict to browse")
        if callback is None:
            instproxy_upgrade(self._c_client, pkg_path, options._c_node, NULL, NULL, &err)
        else:
            instproxy_upgrade(self._c_client, pkg_path, options._c_node, instproxy_notify_cb, <void*>callback, &err)
        handle_error(err)

    cpdef uninstall(self, bytes appid, object client_options, object callback=None):
        cdef:
            plist.Node options
            plist.plist_t c_options
            GError *err = NULL
        if isinstance(client_options, plist.Dict):
            options = client_options
            c_options = options._c_node
        elif isinstance(client_options, dict):
            c_options = plist.native_to_plist_t(client_options)
        else:
            raise TypeError("Must pass a plist.Dict or dict to browse")
        if callback is None:
            instproxy_uninstall(self._c_client, appid, options._c_node, NULL, NULL, &err)
        else:
            instproxy_uninstall(self._c_client, appid, options._c_node, instproxy_notify_cb, <void*>callback, &err)
        handle_error(err)

    cpdef plist.Node lookup_archives(self, object client_options):
        cdef:
            plist.Node options
            plist.plist_t c_options
            plist.plist_t c_node = NULL
            GError *err = NULL
        if isinstance(client_options, plist.Dict):
            options = client_options
            c_options = options._c_node
        elif isinstance(client_options, dict):
            c_options = plist.native_to_plist_t(client_options)
        else:
            raise TypeError("Must pass a plist.Dict or dict to browse")
        c_node = instproxy_lookup_archives(self._c_client, options._c_node, &err)
        try:
            handle_error(err)
        except Exception, e:
            if c_node != NULL:
                plist.plist_free(c_node)
        return plist.plist_t_to_node(c_node)

    cpdef archive(self, bytes appid, object client_options, object callback=None):
        cdef:
            plist.Node options
            plist.plist_t c_options
            GError *err = NULL
        if isinstance(client_options, plist.Dict):
            options = client_options
            c_options = options._c_node
        elif isinstance(client_options, dict):
            c_options = plist.native_to_plist_t(client_options)
        else:
            raise TypeError("Must pass a plist.Dict or dict to browse")
        if callback is None:
            instproxy_archive(self._c_client, appid, options._c_node, NULL, NULL, &err)
        else:
            instproxy_archive(self._c_client, appid, options._c_node, instproxy_notify_cb, <void*>callback, &err)
        handle_error(err)

    cpdef restore(self, bytes appid, object client_options, object callback=None):
        cdef:
            plist.Node options
            plist.plist_t c_options
            GError *err = NULL
        if isinstance(client_options, plist.Dict):
            options = client_options
            c_options = options._c_node
        elif isinstance(client_options, dict):
            c_options = plist.native_to_plist_t(client_options)
        else:
            raise TypeError("Must pass a plist.Dict or dict to browse")
        if callback is None:
            instproxy_restore(self._c_client, appid, options._c_node, NULL, NULL, &err)
        else:
            instproxy_restore(self._c_client, appid, options._c_node, instproxy_notify_cb, <void*>callback, &err)
        handle_error(err)

    cpdef remove_archive(self, bytes appid, object client_options, object callback=None):
        cdef:
            plist.Node options
            plist.plist_t c_options
            GError *err = NULL
        if isinstance(client_options, plist.Dict):
            options = client_options
            c_options = options._c_node
        elif isinstance(client_options, dict):
            c_options = plist.native_to_plist_t(client_options)
        else:
            raise TypeError("Must pass a plist.Dict or dict to browse")
        if callback is None:
            instproxy_remove_archive(self._c_client, appid, options._c_node, NULL, NULL, &err)
        else:
            instproxy_remove_archive(self._c_client, appid, options._c_node, instproxy_notify_cb, <void*>callback, &err)
        handle_error(err)
