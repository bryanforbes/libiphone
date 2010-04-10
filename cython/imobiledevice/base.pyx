cdef class Error(Exception):
    def __cinit__(self, int16_t errcode):
        self._c_errcode = errcode

    def __nonzero__(self):
        return self._c_errcode != 0

    property message:
        def __get__(self):
            return self._lookup_table[self._c_errcode]

    property code:
        def __get__(self):
            return self._c_errcode

    def __str__(self):
        return '%s (%s)' % (self.message, self.code)

    def __repr__(self):
        return self.__str__()

cdef class Base:
    cdef inline int handle_error(self, int16_t ret) except -1:
        if ret == 0:
            return 0
        cdef Error err = self._error(ret)
        raise err
        return -1

    cdef inline Error _error(self, int16_t ret): pass

cdef class Service(Base):
    __service_name__ = None

cdef class PropertyListService(Service):
    cpdef send(self, plist.Node node):
        self.handle_error(self._send(node._c_node))

    cpdef object receive(self):
        cdef:
            plist.plist_t c_node = NULL
            int16_t err
        err = self._receive(&c_node)
        try:
            self.handle_error(err)
        except Error, e:
            if c_node != NULL:
                plist_free(c_node)
            raise

        return plist.plist_t_to_node(c_node)

    cdef inline int16_t _send(self, plist.plist_t node):
        raise NotImplementedError("send is not implemented")

    cdef inline int16_t _receive(self, plist.plist_t* c_node):
        raise NotImplementedError("receive is not implemented")

cdef class DeviceLinkService(PropertyListService):
    pass
