
CFLAGS = -Wall

SRC=$(wildcard *.c */*.c */*/*.c)

all: $(SRC)
	gcc -Wno-incompatible-pointer-types-discards-qualifiers -o ../build/npz $^ $(CFLAGS) $(LIBS)

run:
	make all
	./main
	make clean

clean:
	rm main
