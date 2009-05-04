# wxChart does not use Automake, but the Anjuta IDE reads this file
# to determine which files are in the project, so this file exists to
# make Boxi development with Anjuta easier.

EXTRA_DIST = \
	$(includes_HEADERS) \
	$(sources_DATA) \
	$(other_DATA)

other_DATA = \
	include/wx/chartart/chart_zin.bmp \
	include/wx/chartart/chart_zot.xpm \
	include/wx/chartart/chart_zot.bmp \
	include/wx/chartart/chart_zin.xpm

includes_HEADERS = \	
	include/wx/chartdef.h \
	include/wx/legendwindow.h \
	include/wx/pie3dchartpoints.h \
	include/wx/xaxiswindow.h \
	include/wx/chartcolors.h \
	include/wx/label.h \
	include/wx/chartpoints.h \
	include/wx/chartwindow.h \
	include/wx/yaxis.h \
	include/wx/axis.h \
	include/wx/xaxis.h \
	include/wx/chart.h \
	include/wx/chartctrl.h \
	include/wx/points.h \
	include/wx/charttypes.h \
	include/wx/chartpointstypes.h \
	include/wx/piechartpoints.h \
	include/wx/yaxiswindow.h \
	include/wx/barchartpoints.h \
	include/wx/legend.h \
	include/wx/bar3dchartpoints.h

sources_DATA = \
	src/chartcolors.cpp \
	src/barchartpoints.cpp \
	src/yaxis.cpp \
	src/points.cpp \
	src/piechartpoints.cpp \
	src/chartctrl.cpp \
	src/legend.cpp \
	src/axis.cpp \
	src/chartwindow.cpp \
	src/label.cpp \
	src/bar3dchartpoints.cpp \
	src/pie3dchartpoints.cpp \
	src/legendwindow.cpp \
	src/chart.cpp \
	src/xaxis.cpp \
	src/xaxiswindow.cpp \
	src/yaxiswindow.cpp

lib_LTLIBRARIES = \
	libwxchart.la

libwxchart_la_SOURCES = \
	src/axis.cpp   \
	src/bar3dchartpoints.cpp   \
	src/barchartpoints.cpp   \
	src/chart.cpp   \
	src/chartcolors.cpp   \
	src/chartctrl.cpp   \
	src/chartwindow.cpp   \
	src/label.cpp   \
	src/legend.cpp   \
	src/legendwindow.cpp   \
	src/pie3dchartpoints.cpp   \
	src/piechartpoints.cpp   \
	src/points.cpp   \
	src/xaxis.cpp   \
	src/xaxiswindow.cpp   \
	src/yaxis.cpp   \
	src/yaxiswindow.cpp
