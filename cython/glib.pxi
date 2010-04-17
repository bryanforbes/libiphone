cdef extern from "glib.h":
    ctypedef unsigned int guint32
    ctypedef guint32 GQuark
    ctypedef int    gint
    ctypedef gint   gboolean
    ctypedef char   gchar
    cdef struct _GError:
        GQuark domain
        gint code
        gchar *message
    ctypedef _GError GError

from python_ref cimport PyObject

cdef extern from "pygtk-2.0/pyglib.h":
    void pyglib_init()
    gboolean pyglib_error_check(GError **error)
    object pyglib_register_exception_for_domain(gchar *name, gint error_domain)
