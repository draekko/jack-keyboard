# - Try to find Jack library
# Once done this will define:
#  JACK_FOUND        - system has JACK
#  JACK_INCLUDE_DIR  - incude paths to use
#  JACK_LIBRARIES    - Link these to use

SET(JACK_FOUND 0)

FIND_PATH(JACK_INCLUDE_DIR jack/ringbuffer.h
	/usr/local/include
	/usr/include
)

FIND_LIBRARY(JACK_LIBRARIES jack
	/usr/local/lib
	/usr/lib
)

# handle the QUIETLY and REQUIRED arguments and set JACK_FOUND to TRUE
# if all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(JACK DEFAULT_MSG JACK_LIBRARIES JACK_INCLUDE_DIR)

MARK_AS_ADVANCED(
  JACK_INCLUDE_DIR
  JACK_LIBRARIES
)

