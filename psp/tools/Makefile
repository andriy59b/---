CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c99
PURE = ../src/gfx.c ../src/texgen.c ../src/city.c ../src/car.c ../src/carmodel.c ../src/actor.c ../src/ride.c ../src/cutscene.c ../src/synth.c

preview: preview.c raster.c $(PURE)
	$(CC) $(CFLAGS) -o $@ preview.c raster.c $(PURE) -lz -lm

clean:
	rm -f preview *.png

.PHONY: clean
