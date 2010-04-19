cdef extern from "libimobiledevice/file_relay.h":
    cdef struct file_relay_client_private:
        pass
    ctypedef file_relay_client_private *file_relay_client_t
    ctypedef char** const_sources_t "const char**"

    ctypedef enum FileRelayClientErrorEnum:
        FILE_RELAY_E_SUCCESS = 0
        FILE_RELAY_E_INVALID_ARG = -1
        FILE_RELAY_E_PLIST_ERROR = -2
        FILE_RELAY_E_MUX_ERROR = -3
        FILE_RELAY_E_INVALID_SOURCE = -4
        FILE_RELAY_E_STAGING_EMPTY = -5
        FILE_RELAY_E_UNKNOWN_ERROR = -256

    GQuark file_relay_client_error_quark()
    file_relay_client_t file_relay_client_new(idevice_t device, uint16_t port, GError **error)
    void file_relay_client_free(file_relay_client_t client, GError **error)
    idevice_connection_t file_relay_request_sources(file_relay_client_t client, char **sources, GError **error)

cimport stdlib

FileRelayClientError = pyglib_register_exception_for_domain(
    "imobiledevice.FileRelayClientError", file_relay_client_error_quark())

cdef class FileRelayClient(PropertyListService):
    __service_name__ = "com.apple.mobile.file_relay"
    cdef file_relay_client_t _c_client

    def __cinit__(self, iDevice device not None, int port, *args, **kwargs):
        cdef GError *err = NULL
        self._c_client = file_relay_client_new(device._c_dev, port, &err)
        handle_error(err)

    def __dealloc__(self):
        cdef GError *err = NULL
        if self._c_client is not NULL:
            file_relay_client_free(self._c_client, &err)
            handle_error(err)

    cpdef iDeviceConnection request_sources(self, list sources):
        cdef:
            GError *err = NULL
            Py_ssize_t count = len(sources)
            char** c_sources = <char**>stdlib.malloc(sizeof(char*) * (count + 1))
            idevice_connection_t c_conn = NULL
            iDeviceConnection conn = iDeviceConnection.__new__(iDeviceConnection)

        for i, value in enumerate(sources):
            c_sources[i] = value
        c_sources[count] = NULL

        c_conn = file_relay_request_sources(self._c_client, <const_sources_t>c_sources, &err)
        try:
            handle_error(err)
            conn = iDeviceConnection.__new__(iDeviceConnection)
            conn._c_connection = c_conn
            return conn
        finally:
            stdlib.free(c_sources)
