cdef extern from *:
    ctypedef unsigned char uint8_t
    ctypedef short int int16_t
    ctypedef unsigned short int uint16_t
    ctypedef unsigned int uint32_t
    ctypedef int int32_t
IF UNAME_MACHINE == 'x86_64':
    ctypedef long int int64_t
    ctypedef unsigned long int uint64_t
ELSE:
    ctypedef long long int int64_t
    ctypedef unsigned long long int uint64_t
