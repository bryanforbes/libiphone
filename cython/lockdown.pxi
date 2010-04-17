cdef extern from "libimobiledevice/lockdown.h":
    ctypedef enum LockdowndClientErrorEnum:
        LOCKDOWN_E_SUCCESS = 0
        LOCKDOWN_E_INVALID_ARG = -1
        LOCKDOWN_E_INVALID_CONF = -2
        LOCKDOWN_E_PLIST_ERROR = -3
        LOCKDOWN_E_PAIRING_FAILED = -4
        LOCKDOWN_E_SSL_ERROR = -5
        LOCKDOWN_E_DICT_ERROR = -6
        LOCKDOWN_E_START_SERVICE_FAILED = -7
        LOCKDOWN_E_NOT_ENOUGH_DATA = -8
        LOCKDOWN_E_SET_VALUE_PROHIBITED = -9
        LOCKDOWN_E_GET_VALUE_PROHIBITED = -10
        LOCKDOWN_E_REMOVE_VALUE_PROHIBITED = -11
        LOCKDOWN_E_MUX_ERROR = -12
        LOCKDOWN_E_ACTIVATION_FAILED = -13
        LOCKDOWN_E_PASSWORD_PROTECTED = -14
        LOCKDOWN_E_NO_RUNNING_SESSION = -15
        LOCKDOWN_E_INVALID_HOST_ID = -16
        LOCKDOWN_E_INVALID_SERVICE = -17
        LOCKDOWN_E_INVALID_ACTIVATION_RECORD = -18
        LOCKDOWN_E_UNKNOWN_ERROR = -256
    GQuark lockdownd_client_error_quark()

    lockdownd_client_t lockdownd_client_new(idevice_t device, char *label, GError **error)
    lockdownd_client_t lockdownd_client_new_with_handshake(idevice_t device, char *label, GError **error)
    void lockdownd_client_free(lockdownd_client_t client, GError **error)

    char* lockdownd_query_type(lockdownd_client_t client, GError **error)
    plist.plist_t lockdownd_get_value(lockdownd_client_t client, char *domain, char *key, GError **error)
    void lockdownd_set_value(lockdownd_client_t client, char *domain, char *key, plist.plist_t value, GError **error)
    void lockdownd_remove_value(lockdownd_client_t client, char *domain, char *key, GError **error)
    uint16_t lockdownd_start_service(lockdownd_client_t client, char *service, GError **error)
    void lockdownd_start_session(lockdownd_client_t client, char *host_id, char **session_id, int *ssl_enabled, GError **error)
    void lockdownd_stop_session(lockdownd_client_t client, char *session_id, GError **error)
    void lockdownd_send(lockdownd_client_t client, plist.plist_t plist, GError **error)
    plist.plist_t lockdownd_receive(lockdownd_client_t client, GError **error)
    void lockdownd_pair(lockdownd_client_t client, lockdownd_pair_record_t pair_record, GError **error)
    void lockdownd_validate_pair(lockdownd_client_t client, lockdownd_pair_record_t pair_record, GError **error)
    void lockdownd_unpair(lockdownd_client_t client, lockdownd_pair_record_t pair_record, GError **error)
    void lockdownd_activate(lockdownd_client_t client, plist.plist_t activation_record, GError **error)
    void lockdownd_deactivate(lockdownd_client_t client, GError **error)
    void lockdownd_enter_recovery(lockdownd_client_t client, GError **error)
    void lockdownd_goodbye(lockdownd_client_t client, GError **error)

cdef class LockdownPairRecord:
    #def __cinit__(self, bytes device_certificate, bytes host_certificate, bytes host_id, bytes root_certificate, *args, **kwargs):
    property device_certificate:
        def __get__(self):
            cdef bytes result = self._c_record.device_certificate
            return result
    property host_certificate:
        def __get__(self):
            cdef bytes result = self._c_record.host_certificate
            return result
    property host_id:
        def __get__(self):
            cdef bytes result = self._c_record.host_id
            return result
    property root_certificate:
        def __get__(self):
            cdef bytes result = self._c_record.root_certificate
            return result

LockdownClientError = pyglib_register_exception_for_domain("imobiledevice.LockdownClientError",
    lockdownd_client_error_quark())

cdef class LockdownClient(PropertyListService):
    def __cinit__(self, iDevice device not None, bytes label="", bool handshake=True, *args, **kwargs):
        cdef:
            iDevice dev = device
            GError *error = NULL
            char* c_label = NULL
        if label:
            c_label = label
        if handshake:
            self._c_client = lockdownd_client_new_with_handshake(dev._c_dev, c_label, &error)
        else:
            self._c_client = lockdownd_client_new(dev._c_dev, c_label, &error)

        handle_error(error)

        self.device = dev

    def __dealloc__(self):
        cdef GError *err = NULL
        if self._c_client is not NULL:
            lockdownd_client_free(self._c_client, &err)
            handle_error(err)

    cpdef bytes query_type(self):
        cdef:
            GError *err = NULL
            char* c_type = NULL
            bytes result
        c_type = lockdownd_query_type(self._c_client, &err)
        try:
            handle_error(err)
        except Exception, e:
            raise
        finally:
            if c_type != NULL:
                result = c_type
                stdlib.free(c_type)

        return result

    cpdef plist.Node get_value(self, bytes domain=None, bytes key=None):
        cdef:
            GError *err = NULL
            plist.plist_t c_node = NULL
            char* c_domain = NULL
            char* c_key = NULL
        if domain is not None:
            c_domain = domain
        if key is not None:
            c_key = key
        c_node = lockdownd_get_value(self._c_client, c_domain, c_key, &err)
        try:
            handle_error(err)
        except Exception, e:
            if c_node != NULL:
                plist.plist_free(c_node)
            raise

        return plist.plist_t_to_node(c_node)

    cpdef set_value(self, bytes domain, bytes key, object value):
        cdef:
            GError *err = NULL
            plist.plist_t c_node = plist.native_to_plist_t(value)
        lockdownd_set_value(self._c_client, domain, key, c_node, &err)
        try:
            handle_error(err)
        except Exception, e:
            raise
        finally:
            if c_node != NULL:
                plist.plist_free(c_node)

    cpdef remove_value(self, bytes domain, bytes key):
        cdef GError *err = NULL
        lockdownd_remove_value(self._c_client, domain, key, &err)
        handle_error(err)

    cpdef uint16_t start_service(self, object service):
        cdef:
            char* c_service_name = NULL
            GError *err = NULL
            uint16_t port = 0

        if hasattr(service, '__service_name__') and \
            service.__service_name__ is not None \
            and isinstance(service.__service_name__, basestring):
            c_service_name = <bytes>service.__service_name__
        elif isinstance(service, basestring):
            c_service_name = <bytes>service
        else:
            raise TypeError("LockdownClient.start_service() takes a BaseService or string as its first argument")
        
        port = lockdownd_start_service(self._c_client, c_service_name, &err)
        try:
            handle_error(err)
        except Exception, e:
            raise

        return port

    cpdef object get_service_client(self, object service_class):
        cdef:
            uint16_t port = 0
            object result

        if not hasattr(service_class, '__service_name__') and \
            not service_class.__service_name__ is not None \
            and not isinstance(service_class.__service_name__, basestring):
            raise TypeError("LockdownClient.get_service_client() takes a BaseService as its first argument")

        port = self.start_service(service_class)
        return service_class(self.device, port)

    cpdef tuple start_session(self, bytes host_id):
        cdef:
            GError *err = NULL
            char* c_session_id = NULL
            bint ssl_enabled
            bytes session_id
        lockdownd_start_session(self._c_client, host_id, &c_session_id, &ssl_enabled, &err)
        try:
            handle_error(err)
        except Exception, e:
            raise
        finally:
            if c_session_id != NULL:
                session_id = c_session_id
                stdlib.free(c_session_id)

        return (session_id, ssl_enabled)

    cpdef stop_session(self, bytes session_id):
        cdef GError *err = NULL
        lockdownd_stop_session(self._c_client, session_id, &err)
        handle_error(err)

    cpdef pair(self, object pair_record=None):
        cdef:
            lockdownd_pair_record_t c_pair_record = NULL
            GError *err = NULL
        if pair_record is not None:
            c_pair_record = (<LockdownPairRecord>pair_record)._c_record
        lockdownd_pair(self._c_client, c_pair_record, &err)
        handle_error(err)

    cpdef validate_pair(self, object pair_record=None):
        cdef:
            lockdownd_pair_record_t c_pair_record = NULL
            GError *err = NULL
        if pair_record is not None:
            c_pair_record = (<LockdownPairRecord>pair_record)._c_record
        lockdownd_validate_pair(self._c_client, c_pair_record, &err)
        handle_error(err)

    cpdef unpair(self, object pair_record=None):
        cdef:
            lockdownd_pair_record_t c_pair_record = NULL
            GError *err = NULL
        if pair_record is not None:
            c_pair_record = (<LockdownPairRecord>pair_record)._c_record
        lockdownd_unpair(self._c_client, c_pair_record, &err)
        handle_error(err)

    cpdef activate(self, plist.Node activation_record):
        cdef GError *err = NULL
        lockdownd_activate(self._c_client, activation_record._c_node, &err)
        handle_error(err)

    cpdef deactivate(self):
        cdef GError *err = NULL
        lockdownd_deactivate(self._c_client, &err)
        handle_error(err)

    cpdef enter_recovery(self):
        cdef GError *err = NULL
        lockdownd_enter_recovery(self._c_client, &err)
        handle_error(err)

    cpdef goodbye(self):
        cdef GError *err = NULL
        lockdownd_goodbye(self._c_client, &err)
        handle_error(err)

    cdef inline _send(self, plist.plist_t node, GError **err):
        lockdownd_send(self._c_client, node, err)

    cdef inline plist.plist_t _receive(self, GError **err):
        return lockdownd_receive(self._c_client, err)
