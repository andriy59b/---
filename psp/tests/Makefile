CC ?= cc
CFLAGS ?= -O1 -g -Wall -Wextra -std=c99 -fsanitize=address,undefined

host_test: host_test.c ../src/world.c ../src/gen.c ../src/mesh.c ../src/textures.c
	$(CC) $(CFLAGS) -o $@ $^ -lm

check: host_test
	./host_test

clean:
	rm -f host_test

.PHONY: check clean
