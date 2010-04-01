cdef extern from "libimobiledevice/sbservices.h":
    cdef struct sbservices_client_private:
        pass
    ctypedef sbservices_client_private *sbservices_client_t
    ctypedef enum sbservices_error_t:
        SBSERVICES_E_SUCCESS = 0
        SBSERVICES_E_INVALID_ARG = -1
        SBSERVICES_E_PLIST_ERROR = -2
        SBSERVICES_E_CONN_FAILED = -3
        SBSERVICES_E_UNKNOWN_ERROR = -256
    sbservices_error_t sbservices_client_new(idevice_t device, uint16_t port, sbservices_client_t *client)
    sbservices_error_t sbservices_client_free(sbservices_client_t client)
    sbservices_error_t sbservices_get_icon_state(sbservices_client_t client, plist.plist_t *state)
    sbservices_error_t sbservices_set_icon_state(sbservices_client_t client, plist.plist_t newstate)
    sbservices_error_t sbservices_get_icon_pngdata(sbservices_client_t client, char *bundleId, char **pngdata, uint64_t *pngsize)

cdef class SpringboardServicesError(BaseError):
    def __init__(self, *args, **kwargs):
        self._lookup_table = {
            SBSERVICES_E_SUCCESS: "Success",
            SBSERVICES_E_INVALID_ARG: "Invalid argument",
            SBSERVICES_E_PLIST_ERROR: "Property list error",
            SBSERVICES_E_CONN_FAILED: "Connection failed",
            SBSERVICES_E_UNKNOWN_ERROR: "Unknown error"
        }
        BaseError.__init__(self, *args, **kwargs)

cdef class SpringboardServicesClient(Base):
    cdef sbservices_client_t _c_client

    def __cinit__(self, iDevice device not None, LockdownClient lockdown=None, *args, **kwargs):
        cdef:
            iDevice dev = device
            LockdownClient lckd
        if lockdown is None:
            lckd = LockdownClient(dev)
        else:
            lckd = lockdown
        port = lckd.start_service("com.apple.springboardservices")
        err = SpringboardServicesError(sbservices_client_new(dev._c_dev, port, &(self._c_client)))
        if err: raise err
    
    def __dealloc__(self):
        if self._c_client is not NULL:
            err = SpringboardServicesError(sbservices_client_free(self._c_client))
            if err: raise err

    cdef inline BaseError _error(self, int16_t ret):
        return SpringboardServicesError(ret)

    property icon_state:
        def __get__(self):
            cdef:
                plist.plist_t c_node = NULL
                plist.Node node
                sbservices_error_t err
            err = sbservices_get_icon_state(self._c_client, &c_node)
            try:
                self.handle_error(err)
            except BaseError, e:
                if c_node != NULL:
                    plist_free(c_node)
                raise
            node = plist.plist_t_to_node(c_node)
            return node
        def __set__(self, plist.Node newstate not None):
            cdef plist.Node node = newstate
            self.handle_error(sbservices_set_icon_state(self._c_client, node._c_node))

    cpdef bytes get_pngdata(self, bytes bundleId):
        cdef:
            bytes result
            char* pngdata = NULL
            uint64_t pngsize
            sbservices_error_t err
        err = sbservices_get_icon_pngdata(self._c_client, bundleId, &pngdata, &pngsize)
        try:
            self.handle_error(err)
        except BaseError, e:
            raise
        finally:
            result = pngdata[:pngsize]
            free(pngdata)
        return result
