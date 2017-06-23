#AM_LIBS = $(DEPS_LIBS)


AM_CFLAGS= -I$(top_srcdir)/src/include -g
lib_LTLIBRARIES = libxambit.la
libxambit_la_SOURCES = src/xambit.c
include_HEADERS = src/include/xambit.h

bin_SCRIPTS = tools/xambit_xts_init_cg.sh

nobase_noinst_PROGRAMS = examples/dropbox/dbsend examples/dropbox/dbrec examples/ais/aissend examples/ais/aisrec

examples_dropbox_dbsend_SOURCES = examples/dropbox/dropbox_sender.c src/include/xambit.h
examples_dropbox_dbsend_LDADD = libxambit.la

examples_dropbox_dbrec_SOURCES = examples/dropbox/dropbox_receiver.c src/include/xambit.h
examples_dropbox_dbrec_LDADD = libxambit.la

examples_ais_aissend_SOURCES = examples/ais/ais_send.c src/include/xambit.h
examples_ais_aissend_LDADD = libxambit.la

examples_ais_aisrec_SOURCES = examples/ais/ais_rec.c src/include/xambit.h
examples_ais_aisrec_LDADD = libxambit.la

man_MANS = man/channel_close.3 man/channel_fifo_open.3 man/channel_receive.3 man/channel_receive_to_file.3 man/channel_register_type.3 man/channel_send.3 man/channel_send_file.3 man/xambit_parcel_hdr_t.3

#xambit_CPPFLAGS = -DDEBUG
