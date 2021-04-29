CFLAGS := -std=c99 -Wall -Wextra
LDFLAGS := -lm -lmpi -lX11

all:
	$(shell mpicc -showme) ${CFLAGS} src/mpi_x11blit.c -o \
	mpi_x11blit ${LDFLAGS}

clean:
	rm -f mpi_x11blit