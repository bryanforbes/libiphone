cdef extern from "libimobiledevice/screenshotr.h":
    cdef struct screenshotr_client_private:
        pass
    ctypedef screenshotr_client_private *screenshotr_client_t

    ctypedef enum screenshotr_error_t:
        SCREENSHOTR_E_SUCCESS = 0
        SCREENSHOTR_E_INVALID_ARG = -1
        SCREENSHOTR_E_PLIST_ERROR = -2
        SCREENSHOTR_E_MUX_ERROR = -3
        SCREENSHOTR_E_BAD_VERSION = -4
        SCREENSHOTR_E_UNKNOWN_ERROR = -256

    GQuark screenshotr_client_error_quark()
    screenshotr_client_t screenshotr_client_new(idevice_t device, uint16_t port, GError **error)
    void screenshotr_client_free(screenshotr_client_t client, GError **error)
    void screenshotr_take_screenshot(screenshotr_client_t client, char **imgdata, uint64_t *imgsize, GError **error)

ScreenshotrClientError = pyglib_register_exception_for_domain(
    "imobiledevice.ScreenshotrClientError", screenshotr_client_error_quark())

cdef class ScreenshotrClient(DeviceLinkService):
    __service_name__ = "com.apple.mobile.screenshotr"
    cdef screenshotr_client_t _c_client

    def __cinit__(self, iDevice device not None, int port, *args, **kwargs):
        cdef GError *err = NULL
        self._c_client = screenshotr_client_new(device._c_dev, port, &err)
        handle_error(err)

    def __dealloc__(self):
        cdef GError *err = NULL
        if self._c_client is not NULL:
            screenshotr_client_free(self._c_client, &err)
            handle_error(err)

    cpdef bytes take_screenshot(self):
        cdef:
            char* c_data
            uint64_t data_size
            bytes result
            GError *err = NULL

        screenshotr_take_screenshot(self._c_client, &c_data, &data_size, &err)
        handle_error(err)
        result = c_data[:data_size]
        return result
