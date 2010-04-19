cdef extern from "libimobiledevice/sbservices.h":
    cdef struct sbservices_client_private:
        pass
    ctypedef sbservices_client_private *sbservices_client_t
    ctypedef enum SBServicesClientErrorEnum:
        SBSERVICES_E_SUCCESS = 0
        SBSERVICES_E_INVALID_ARG = -1
        SBSERVICES_E_PLIST_ERROR = -2
        SBSERVICES_E_CONN_FAILED = -3
        SBSERVICES_E_UNKNOWN_ERROR = -256
    GQuark sbservices_client_error_quark()

    sbservices_client_t sbservices_client_new(idevice_t device, uint16_t port, GError **error)
    void sbservices_client_free(sbservices_client_t client, GError **error)
    plist.plist_t sbservices_get_icon_state(sbservices_client_t client, GError **error)
    void sbservices_set_icon_state(sbservices_client_t client, plist.plist_t newstate, GError **error)
    void sbservices_get_icon_pngdata(sbservices_client_t client, char *bundleId, char **pngdata, uint64_t *pngsize, GError **error)

SpringboardServicesClientError = pyglib_register_exception_for_domain("imobiledevice.SpringboardServicesClientError",
    sbservices_client_error_quark())

cdef class SpringboardServicesClient(PropertyListService):
    __service_name__ = "com.apple.springboardservices"
    cdef sbservices_client_t _c_client

    def __cinit__(self, iDevice device not None, int port, *args, **kwargs):
        cdef GError *err = NULL
        self._c_client = sbservices_client_new(device._c_dev, port, &err)
        handle_error(err)
    
    def __dealloc__(self):
        cdef GError *err = NULL
        if self._c_client is not NULL:
            sbservices_client_free(self._c_client, &err)
            handle_error(err)

    property icon_state:
        def __get__(self):
            cdef:
                plist.plist_t c_node = NULL
                GError *err = NULL
            c_node = sbservices_get_icon_state(self._c_client, &err)
            try:
                handle_error(err)

                return plist.plist_t_to_node(c_node)
            except Exception, e:
                if c_node != NULL:
                    plist.plist_free(c_node)
                raise
        def __set__(self, plist.Node newstate not None):
            cdef GError *err = NULL
            sbservices_set_icon_state(self._c_client, newstate._c_node, &err)
            handle_error(err)

    cpdef bytes get_pngdata(self, bytes bundleId):
        cdef:
            char* pngdata = NULL
            uint64_t pngsize
            GError *err = NULL
        sbservices_get_icon_pngdata(self._c_client, bundleId, &pngdata, &pngsize, &err)
        try:
            handle_error(err)

            return pngdata[:pngsize]
        except Exception, e:
            stdlib.free(pngdata)
            raise
