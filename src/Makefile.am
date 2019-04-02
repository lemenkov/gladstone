plugin_LTLIBRARIES = libgstg729.la

# sources used to compile this plug-in
libgstg729_la_SOURCES = gstg729plugin.c gstg729enc.c gstg729dec.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstg729_la_CFLAGS = $(GSTPB_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstg729_la_LIBADD = $(GSTPB_BASE_LIBS) -lgstaudio-@GST_MAJORMINOR@ $(GST_BASE_LIBS) $(GST_LIBS)

libgstg729_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstg729_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstg729enc.h gstg729dec.h g729common.h
