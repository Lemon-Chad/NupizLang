
CFLAGS = -Wall -Wno-incompatible-pointer-types-discards-qualifiers
CPPFLAGS = -Wall

SRCDIR = .
OBJDIR = ../lib

C_SRC=$(shell find $(SRCDIR) -name "*.c")
CPP_SRC = $(shell find $(SRCDIR) -name "*.cpp")

C_OBJ=$(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(C_SRC))
CPP_OBJ=$(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(CPP_SRC))

OBJ=$(C_OBJ) $(CPP_OBJ)

TARGET=../build/npz

all: $(OBJDIR) $(TARGET)

rebuild:
	make clean
	make all

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(TARGET): $(OBJ)
	g++ $(OBJ) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p `dirname $@`
	gcc $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	mkdir -p `dirname $@`
	g++ $(CPPFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)
