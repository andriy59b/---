TARGET = ukrainecraft
OBJS = src/main.o src/render.o src/hud.o src/audio.o src/synth.o src/story.o src/ride.o src/crowd.o src/cutscene.o src/actor.o src/car.o src/carmodel.o src/city.o src/gfx.o src/texgen.o

CFLAGS = -O2 -G0 -Wall -Wextra
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS = -lpspgum -lpspgu -lpspaudio -lpsppower -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = UkraineCraft
PSP_LARGE_MEMORY = 1

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
