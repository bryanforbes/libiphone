cdef extern from "libimobiledevice/mobilebackup.h":
    cdef struct mobilebackup_client_private:
        pass
    ctypedef mobilebackup_client_private *mobilebackup_client_t

    ctypedef enum MobileBackupClientErrorEnum:
        MOBILEBACKUP_E_SUCCESS = 0
        MOBILEBACKUP_E_INVALID_ARG = -1
        MOBILEBACKUP_E_PLIST_ERROR = -2
        MOBILEBACKUP_E_MUX_ERROR = -3
        MOBILEBACKUP_E_BAD_VERSION = -4
        MOBILEBACKUP_E_UNKNOWN_ERROR = -256
    GQuark mobilebackup_client_error_quark()

    mobilebackup_client_t mobilebackup_client_new(idevice_t device, uint16_t port, GError **error)
    void mobilebackup_client_free(mobilebackup_client_t client, GError **error)
    plist.plist_t mobilebackup_receive(mobilebackup_client_t client, GError **error)
    void mobilebackup_send(mobilebackup_client_t client, plist.plist_t plist, GError **error)
    void mobilebackup_request_backup(mobilebackup_client_t client, plist.plist_t backup_manifest, char *base_path, char *proto_version, GError **error)
    void mobilebackup_send_backup_file_received(mobilebackup_client_t client, GError **error)
    void mobilebackup_send_error(mobilebackup_client_t client, char *reason, GError **error)

MobileBackupClientError = pyglib_register_exception_for_domain("imobiledevice.MobileBackupClientError",
    mobilebackup_client_error_quark())

cdef class MobileBackupClient(PropertyListService):
    __service_name__ = "com.apple.mobilebackup"
    cdef mobilebackup_client_t _c_client

    def __cinit__(self, iDevice device not None, int port, *args, **kwargs):
        cdef GError *err = NULL
        self._c_client = mobilebackup_client_new(device._c_dev, port, &err)
        handle_error(err)

    def __dealloc__(self):
        cdef GError *err = NULL
        if self._c_client is not NULL:
            mobilebackup_client_free(self._c_client, &err)
            handle_error(err)

    cdef inline _send(self, plist.plist_t node, GError **error):
        mobilebackup_send(self._c_client, node, error)

    cdef inline plist.plist_t _receive(self, GError **error):
        return mobilebackup_receive(self._c_client, error)
