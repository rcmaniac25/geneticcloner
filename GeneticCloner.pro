APP_NAME = GeneticCloner

CONFIG += qt warn_on cascades10
LIBS += -lEGL -lGLESv2 -lscreen -lbb

include(config.pri)

CONFIG(debug) {
	DEFINES += GC_DEBUG
}
