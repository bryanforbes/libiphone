from base cimport Base, Error as BaseError

include "std.pxi"

cdef class Error(BaseError): pass

cdef extern from "libimobiledevice/libimobiledevice.h":
    cdef struct idevice_private:
        pass
    ctypedef idevice_private* idevice_t
    cdef struct idevice_connection_private:
        pass
    ctypedef idevice_connection_private* idevice_connection_t
    cdef enum idevice_event_type:
        IDEVICE_DEVICE_ADD = 1,
        IDEVICE_DEVICE_REMOVE
    ctypedef struct idevice_event_t:
        idevice_event_type event
        char *uuid
        int conn_type
    ctypedef idevice_event_t* const_idevice_event_t "const idevice_event_t*"

cdef class iDeviceEvent:
    cdef const_idevice_event_t _c_event

cdef class iDeviceConnection(Base):
    cdef idevice_connection_t _c_connection

    cpdef disconnect(self)

cdef class iDevice(Base):
    cdef idevice_t _c_dev

    cpdef iDeviceConnection connect(self, uint16_t port)
