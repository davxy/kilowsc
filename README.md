Distributed programming with Kilobots
=====================================

The project consists in the the analysis, modeling and development of a
System of Systems.

In practice it consists in a group of Kilobots robots playing
together a classical social game from the 80/90’s: “Witch says colors”.

The game allows to explore several areas of distributed computing and SoS
management: such as leader election, consensus, information sharing,
networking and resiliency.

The Kilobots resources are limited enough to enforce the application of
embedded programming best practices and often leave the comfort
zone that nowadays many applications programmers gives for guaranteed.

Regardless of the continuous release of more and more powerful high-end
machines, in the IoT world low-cost and low-power devices are still the
industry way-to-go.

The Kilobots 2KB of SRAM and 32KB of flash may seem medieval age numbers
but such values are more common than how you think if you consider that
about zero percent of the world's microprocessors are used in
personal computers.

Every PC, server, supercomputer, and all the other general-purpose computers
put together account for less than 1% of all the microprocessors sold
every year. If you round off the fractions, embedded systems consume
100% of the worldwide production of microprocessors.


Compilation
-----------

## Kilobot target

Dependencies:
- avr-gcc : gcc port for the AVR microcontroller.
- binutils-avr : linker and assembler for the AVR microcontroller.
- avr-libc : standard C library for the AVR microcontroller.
- avrdude : if you intend to flash the filobots program memory.
- kilolib : the official Kilobot library.

For the kilolib compilation plase follow the instructions reported in the [official repository](https://github.com/acornejo/kilolib)

## Kilombo simulator

Dependencies:
- gcc : C compiler
- binutils : linker and assembler
- libs : standard C library
- kilombo : kilobot simulator

For kilombo simulator installation please follow the instructions reported in the [official repository](https://github.com/JIC-CSB/kilombo)

## Compilation options

Configuration via the config.mk file.

Compile for the kilombo simulator

    KILOMBO := y

(Simulator only) Path to kilombo libraries.
    
    KILOMBO_PATH := /home/davxy/dev/kilobot/kilombo/install/usr/local

(Target only) Path to kilolib libraries.
    
    KILOLIB_PATH := /home/davxy/dev/kilobot/kilolib

Skip the leader election procedure and immediately start the HNT subsystem with default group-id = 1

    SKIP_ELECTION := y

Visual trasport protocol layer (led colors)

    VISUAL_TPL := y

Visual application layer visual (led colors)

    VISUAL_APP := y

Verbose transport protocol layer

    VERBOSE_TPL := y

Verbose application layer
    
    VERBOSE_APP := y

Verbose buffer statistics (used for best effort fine tune raw buffers sizes)

    VERBOSE_BUF := y

Quickstart
----------

Assuming all the dependencies satisfied and options correctly set.

    $ make
    $ ./wsc

