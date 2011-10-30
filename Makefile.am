CFLAGS = -g -O3 -Wall

bin_PROGRAMS = orrery
orrery_SOURCES = analemma.c seasons.c planetInfo.c orrery.c orrery.h phaseConstants.h, phenomenaConstants.h jovianSatellites.c astrometry.c vSOP87PlanetInfo.c moonPosition.c
orrery_CPPFLAGS = $(GTK_CFLAGS) $(HILDON_CFLAGS) $(LIBOSSO_CFLAGS) $(DBUS_CFLAGS) -DDATA_DIR="\"${pkgdatadir}\""
orrery_LDADD = $(GTK_LIBS) $(HILDON_LIBS) $(LIBOSSO_LIBS) $(DBUS_LIBS) -lrt

appdir = ${datarootdir}/applications
app_DATA = orrery.desktop

SUBDIRS = icons data maemoFiles
