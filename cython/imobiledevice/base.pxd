cimport plist

include "std.pxi"

cdef extern from "pyerrors.h":
    ctypedef class __builtin__.Exception [object PyBaseExceptionObject]:
        pass

cdef class Error(Exception):
    cdef dict _lookup_table
    cdef int16_t _c_errcode

cdef class Base:
    cdef inline int handle_error(self, int16_t ret) except -1
    cdef inline Error _error(self, int16_t ret)

cdef class Service(Base):
    pass

cdef class PropertyListService(Service):
    cpdef send(self, plist.Node node)
    cpdef object receive(self)
    cdef inline int16_t _send(self, plist.plist_t node)
    cdef inline int16_t _receive(self, plist.plist_t* c_node)

cdef class DeviceLinkService(PropertyListService):
    pass
