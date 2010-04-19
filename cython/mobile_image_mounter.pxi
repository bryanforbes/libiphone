cdef extern from "libimobiledevice/mobile_image_mounter.h":
    cdef struct mobile_image_mounter_client_private:
        pass
    ctypedef mobile_image_mounter_client_private *mobile_image_mounter_client_t

    ctypedef enum MobileImageMounterClientErrorEnum:
        MOBILE_IMAGE_MOUNTER_E_SUCCESS = 0
        MOBILE_IMAGE_MOUNTER_E_INVALID_ARG = -1
        MOBILE_IMAGE_MOUNTER_E_PLIST_ERROR = -2
        MOBILE_IMAGE_MOUNTER_E_CONN_FAILED = -3
        MOBILE_IMAGE_MOUNTER_E_UNKNOWN_ERROR = -256

    GQuark mobile_image_mounter_client_error_quark()
    mobile_image_mounter_client_t mobile_image_mounter_new(idevice_t device, uint16_t port, GError **error)
    void mobile_image_mounter_free(mobile_image_mounter_client_t client, GError **error)
    plist.plist_t mobile_image_mounter_lookup_image(mobile_image_mounter_client_t client, char *image_type, GError **error)
    plist.plist_t mobile_image_mounter_mount_image(mobile_image_mounter_client_t client, char *image_path, char *image_signature, uint16_t signature_length, char *image_type, GError **error)
    void mobile_image_mounter_hangup(mobile_image_mounter_client_t client, GError **error)

MobileImageMounterClientError = pyglib_register_exception_for_domain(
    "imobiledevice.MobileImageMounterClientError", mobile_image_mounter_client_error_quark())

cdef class MobileImageMounterClient(PropertyListService):
    __service_name__ = "com.apple.mobile.mobile_image_mounter"
    cdef mobile_image_mounter_client_t _c_client

    def __cinit__(self, iDevice device not None, int port, *args, **kwargs):
        cdef GError *err = NULL
        self._c_client = mobile_image_mounter_new(device._c_dev, port, &err)
        handle_error(err)
    
    def __dealloc__(self):
        cdef GError *err = NULL
        if self._c_client is not NULL:
            mobile_image_mounter_free(self._c_client, &err)
            handle_error(err)

    cpdef plist.Node lookup_image(self, bytes image_type):
        cdef:
            plist.plist_t c_node = NULL
            GError *err = NULL
        c_node = mobile_image_mounter_lookup_image(self._c_client, image_type, &err)
        try:
            handle_error(err)
        except Exception, e:
            if c_node != NULL:
                plist.plist_free(c_node)
        return plist.plist_t_to_node(c_node)

    cpdef plist.Node mount_image(self, bytes image_path, bytes image_signature, bytes image_type):
        cdef:
            plist.plist_t c_node = NULL
            GError *err = NULL
        c_node = mobile_image_mounter_mount_image(self._c_client, image_path, image_signature, len(image_signature),
                                                  image_type, &err)
        try:
            handle_error(err)
        except Exception, e:
            if c_node != NULL:
                plist.plist_free(c_node)
        return plist.plist_t_to_node(c_node)

    cpdef hangup(self):
        cdef GError *err = NULL
        mobile_image_mounter_hangup(self._c_client, &err)
        handle_error(err)
