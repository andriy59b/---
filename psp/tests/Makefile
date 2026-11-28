CC ?= cc
CFLAGS ?= -O1 -g -Wall -Wextra -std=c99 -fsanitize=address,undefined
SRC = ../src/gfx.c ../src/texgen.c ../src/city.c ../src/car.c ../src/carmodel.c ../src/actor.c \
      ../src/cutscene.c ../src/story.c ../src/synth.c ../src/ride.c ../src/crowd.c

host_test: host_test.c $(SRC)
	$(CC) $(CFLAGS) -o $@ host_test.c $(SRC) -lm

check: host_test
	./host_test

clean:
	rm -f host_test test_voice.wav

.PHONY: check clean
