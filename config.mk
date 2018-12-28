# Compile for the kilombo simulator
KILOMBO := y

# Path to the kilolib
# For the kilombo simulator should be the path of kilombo installatin dir
# To install kilombo in a non systemwide directory:
#     make DESTDIR=<destpath> install 
KILO_PATH := /home/davxy/dev/kilobot/kilombo/install/usr/local

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
