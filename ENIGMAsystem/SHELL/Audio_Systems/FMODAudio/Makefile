SOURCES += $(wildcard Audio_Systems/FMODAudio/*.cpp)

ifeq ($(PLATFORM), Win32)
	override LDLIBS += additional/FMOD/lib/libfmodex.a ## Fix Me: additional is missing
endif
