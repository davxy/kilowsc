# Compile for the kilombo simulator
KILOMBO := y

# (Simulator only)
# Path to kilombo libraries.
# For the kilombo simulator should be the path of kilombo installation dir
# To install kilombo in a non systemwide directory:
#     make DESTDIR=<destpath> install 
KILOMBO_PATH := /home/davxy/dev/kilobot/kilombo/install/usr/local

# (Target only)
# Path to kilolib libraries.
KILOLIB_PATH := /home/davxy/dev/kilobot/kilolib

# Skip the election procedure
#SKIP_ELECTION := y

# Visual trasport protocol layer (led colors)
#VISUAL_TPL := y

# Visual application layer visual (led colors)
VISUAL_APP := y

# Verbose transport protocol layer
#VERBOSE_TPL := y

# Verbose application layer
VERBOSE_APP := y

# Verbose buffer statistics (used for best effort fine tune raw buffers sizes)
#VERBOSE_BUF := y
