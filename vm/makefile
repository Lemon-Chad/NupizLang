
CFLAGS = -Wall

SRC=$(wildcard *.c)

all: $(SRC)
	gcc -Wno-incompatible-pointer-types-discards-qualifiers -o main $^ $(CFLAGS) $(LIBS)

run:
	make all
	./main
	make clean

clean:
	rm main
