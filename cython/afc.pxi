cdef extern from "libimobiledevice/afc.h":
    cdef struct afc_client_private:
        pass
    ctypedef afc_client_private *afc_client_t
    ctypedef enum AfcClientErrorEnum:
        AFC_E_SUCCESS = 0
        AFC_E_UNKNOWN_ERROR = 1
        AFC_E_OP_HEADER_INVALID = 2
        AFC_E_NO_RESOURCES = 3
        AFC_E_READ_ERROR = 4
        AFC_E_WRITE_ERROR = 5
        AFC_E_UNKNOWN_PACKET_TYPE = 6
        AFC_E_INVALID_ARG = 7
        AFC_E_OBJECT_NOT_FOUND = 8
        AFC_E_OBJECT_IS_DIR = 9
        AFC_E_PERM_DENIED = 10
        AFC_E_SERVICE_NOT_CONNECTED = 11
        AFC_E_OP_TIMEOUT = 12
        AFC_E_TOO_MUCH_DATA = 13
        AFC_E_END_OF_DATA = 14
        AFC_E_OP_NOT_SUPPORTED = 15
        AFC_E_OBJECT_EXISTS = 16
        AFC_E_OBJECT_BUSY = 17
        AFC_E_NO_SPACE_LEFT = 18
        AFC_E_OP_WOULD_BLOCK = 19
        AFC_E_IO_ERROR = 20
        AFC_E_OP_INTERRUPTED = 21
        AFC_E_OP_IN_PROGRESS = 22
        AFC_E_INTERNAL_ERROR = 23
        AFC_E_MUX_ERROR = 30
        AFC_E_NO_MEM = 31
        AFC_E_NOT_ENOUGH_DATA = 32
        AFC_E_DIR_NOT_EMPTY = 33
    ctypedef enum afc_file_mode_t:
        AFC_FOPEN_RDONLY   = 0x00000001
        AFC_FOPEN_RW       = 0x00000002
        AFC_FOPEN_WRONLY   = 0x00000003
        AFC_FOPEN_WR       = 0x00000004
        AFC_FOPEN_APPEND   = 0x00000005
        AFC_FOPEN_RDAPPEND = 0x00000006
    ctypedef enum afc_link_type_t:
        AFC_HARDLINK = 1
        AFC_SYMLINK = 2
    ctypedef enum afc_lock_op_t:
        AFC_LOCK_SH = 1 | 4
        AFC_LOCK_EX = 2 | 4
        AFC_LOCK_UN = 8 | 4

    GQuark afc_client_error_quark()

    afc_client_t afc_client_new(idevice_t device, uint16_t port, GError **error)
    void afc_client_free(afc_client_t client, GError **error)

    char** afc_get_device_info(afc_client_t client, GError **error)
    char** afc_read_directory(afc_client_t client, char *dir, GError **error)
    char** afc_get_file_info(afc_client_t client, char *filename, GError **error)
    uint64_t afc_file_open(afc_client_t client, char *filename, afc_file_mode_t file_mode, GError **error)
    void afc_file_close(afc_client_t client, uint64_t handle, GError **error)
    void afc_file_lock(afc_client_t client, uint64_t handle, afc_lock_op_t operation, GError **error)
    uint32_t afc_file_read(afc_client_t client, uint64_t handle, char *data, uint32_t length, GError **error)
    uint32_t afc_file_write(afc_client_t client, uint64_t handle, char *data, uint32_t length, GError **error)
    void afc_file_seek(afc_client_t client, uint64_t handle, int64_t offset, int whence, GError **error)
    uint64_t afc_file_tell(afc_client_t client, uint64_t handle, GError **error)
    void afc_file_truncate(afc_client_t client, uint64_t handle, uint64_t newsize, GError **error)
    void afc_remove_path(afc_client_t client, char *path, GError **error)
    void afc_rename_path(afc_client_t client, char *fr, char *to, GError **error)
    void afc_make_directory(afc_client_t client, char *dir, GError **error)
    void afc_truncate(afc_client_t client, char *path, uint64_t newsize, GError **error)
    void afc_make_link(afc_client_t client, afc_link_type_t linktype, char *target, char *linkname, GError **error)
    void afc_set_file_time(afc_client_t client, char *path, uint64_t mtime, GError **error)

    char* afc_get_device_info_key(afc_client_t client, char *key, GError **error)

LOCK_SH = AFC_LOCK_SH
LOCK_EX = AFC_LOCK_EX
LOCK_UN = AFC_LOCK_UN

AfcClientError = pyglib_register_exception_for_domain("imobiledevice.AfcClientError",
    afc_client_error_quark())

# forward declaration of AfcClient
cdef class AfcClient(BaseService)

cdef class AfcFile(object):
    cdef uint64_t _c_handle
    cdef AfcClient _client
    cdef bytes _filename

    def __init__(self, *args, **kwargs):
        raise TypeError("AfcFile cannot be instantiated")

    cpdef close(self):
        cdef GError *err = NULL
        afc_file_close(self._client._c_client, self._c_handle, &err)
        handle_error(err)

    cpdef lock(self, int operation):
        cdef GError *err = NULL
        afc_file_lock(self._client._c_client, self._c_handle, <afc_lock_op_t>operation, &err)
        handle_error(err)

    cpdef seek(self, int64_t offset, int whence):
        cdef GError *err = NULL
        afc_file_seek(self._client._c_client, self._c_handle, offset, whence, &err)
        handle_error(err)

    cpdef uint64_t tell(self):
        cdef:
            uint64_t position
            GError *err = NULL
        position = afc_file_tell(self._client._c_client, self._c_handle, &err)
        handle_error(err)
        return position

    cpdef truncate(self, uint64_t newsize):
        cdef GError *err = NULL
        afc_file_truncate(self._client._c_client, self._c_handle, newsize, &err)
        handle_error(err)

    cpdef uint32_t write(self, bytes data):
        cdef:
            uint32_t bytes_written
            GError *err = NULL
        bytes_written = afc_file_write(self._client._c_client, self._c_handle, data, len(data), &err)
        handle_error(err)

        return bytes_written

cdef class AfcClient(BaseService):
    __service_name__ = "com.apple.afc"
    cdef afc_client_t _c_client

    def __cinit__(self, iDevice device not None, int port, *args, **kwargs):
        cdef GError *err = NULL
        self._c_client = afc_client_new(device._c_dev, port, &err)
        handle_error(err)
    
    def __dealloc__(self):
        cdef GError *err = NULL
        if self._c_client is not NULL:
            afc_client_free(self._c_client, &err)
            handle_error(err)

    cpdef list get_device_info(self):
        cdef:
            GError *err = NULL
            char** infos
            bytes info
            int i = 0
            list result = []
        infos = afc_get_device_info(self._c_client, &err)
        try:
            handle_error(err)
        except Exception, e:
            raise
        finally:
            if infos != NULL:
                while infos[i]:
                    info = infos[i]
                    result.append(info)
                    stdlib.free(infos[i])
                    i = i + 1
                stdlib.free(infos)

        return result

    cpdef list read_directory(self, bytes directory):
        cdef:
            GError *err = NULL
            char** dir_list
            bytes f
            int i = 0
            list result = []
        dir_list = afc_read_directory(self._c_client, directory, &err)
        try:
            handle_error(err)
        except Exception, e:
            raise
        finally:
            if dir_list != NULL:
                while dir_list[i]:
                    f = dir_list[i]
                    result.append(f)
                    stdlib.free(dir_list[i])
                    i = i + 1
                stdlib.free(dir_list)

        return result

    cpdef AfcFile open(self, bytes filename, bytes mode='r'):
        cdef:
            GError *err = NULL
            afc_file_mode_t c_mode
            uint64_t handle
            AfcFile f
        if mode == <bytes>'r':
            c_mode = AFC_FOPEN_RDONLY
        elif mode == <bytes>'r+':
            c_mode = AFC_FOPEN_RW
        elif mode == <bytes>'w':
            c_mode = AFC_FOPEN_WRONLY
        elif mode == <bytes>'w+':
            c_mode = AFC_FOPEN_WR
        elif mode == <bytes>'a':
            c_mode = AFC_FOPEN_APPEND
        elif mode == <bytes>'a+':
            c_mode = AFC_FOPEN_RDAPPEND
        else:
            raise ValueError("mode string must be 'r', 'r+', 'w', 'w+', 'a', or 'a+'")

        handle = afc_file_open(self._c_client, filename, c_mode, &err)
        handle_error(err)
        f = AfcFile.__new__(AfcFile)
        f._c_handle = handle
        f._client = self
        f._filename = filename

        return f

    cpdef get_file_info(self, bytes path):
        cdef:
            GError *err = NULL
            list result
            char** c_result
            int i = 0
            bytes info
        try:
            c_result = afc_get_file_info(self._c_client, path, &err)
        except Exception, e:
            raise
        finally:
            if c_result != NULL:
                while c_result[i]:
                    info = c_result[i]
                    result.append(info)
                    stdlib.free(c_result[i])
                    i = i + 1
                stdlib.free(c_result)

        return result

    cpdef remove_path(self, bytes path):
        cdef GError *err = NULL
        afc_remove_path(self._c_client, path, &err)
        handle_error(err)

    cpdef rename_path(self, bytes f, bytes t):
        cdef GError *err = NULL
        afc_rename_path(self._c_client, f, t, &err)
        handle_error(err)

    cpdef make_directory(self, bytes d):
        cdef GError *err = NULL
        afc_make_directory(self._c_client, d, &err)
        handle_error(err)

    cpdef truncate(self, bytes path, uint64_t newsize):
        cdef GError *err = NULL
        afc_truncate(self._c_client, path, newsize, &err)
        handle_error(err)

    cpdef link(self, bytes source, bytes link_name):
        cdef GError *err = NULL
        afc_make_link(self._c_client, AFC_HARDLINK, source, link_name, &err)
        handle_error(err)

    cpdef symlink(self, bytes source, bytes link_name):
        cdef GError *err = NULL
        afc_make_link(self._c_client, AFC_SYMLINK, source, link_name, &err)
        handle_error(err)

    cpdef set_file_time(self, bytes path, uint64_t mtime):
        cdef GError *err = NULL
        afc_set_file_time(self._c_client, path, mtime, &err)
        handle_error(err)
