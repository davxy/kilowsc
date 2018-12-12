#
# Path to the kilolib
#
# For the kilombo simulator should be the path of kilombo installatin dir
#
# To install kilombo in a non systemwide directory:
# 	make DESTDIR=<destpath> install
# 

KILO_PATH := /home/davxy/dev/kilobot/kilombo/install/usr/local

CC := gcc

CPPFLAGS := -I$(KILO_PATH)/include -DSIM
CFLAGS := -g -O0 -Wall -pedantic
LDLIBS := -L$(KILO_PATH)/lib -lsim -lSDL -lm -ljansson

.PHONY: all clean

objects := app.o buf.o chan.o discover.o spt.o color.o wsc.o

target := wsc

all: $(target)

clean:
	$(RM) $(target) $(objects)

$(target): $(objects)
	$(CC) $(CFLAGS) $(LDLIBS) -o $@ $^
