# Compile for the kilombo simulator
KILOMBO := y

# Path to the kilolib
# For the kilombo simulator should be the path of kilombo installatin dir
# To install kilombo in a non systemwide directory:
#     make DESTDIR=<destpath> install 
KILO_PATH := /home/davxy/dev/kilobot/kilombo/install/usr/local

# Skip the election procedure
#SKIP_ELECTION := y

# Trasport layer visual (led colors) feedback
#VISUAL_CHAN := y

# Application layer visual (led colors) feedback
VISUAL_APP := y

# Verbose transport layer
#VERBOSE_CHAN := y

# Verbose application layer
VERBOSE_APP := y