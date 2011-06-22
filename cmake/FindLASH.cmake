# - Try to find Lash library
# Once done this will define:
#  LASH_FOUND        - system has LASH
#  LASH_INCLUDE_DIR  - incude paths to use
#  LASH_LIBRARIES    - Link these to use

SET(LASH_FOUND 0)

FIND_PATH(LASH_INCLUDE_DIR lash/lash.h
	/usr/local/include/lash-1.0
	/usr/include/lash-1.0
)

FIND_LIBRARY(LASH_LIBRARIES lash
	/usr/local/lib
	/usr/lib
)

# handle the QUIETLY and REQUIRED arguments and set LASH_FOUND to TRUE
# if all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LASH DEFAULT_MSG LASH_LIBRARIES LASH_INCLUDE_DIR)

MARK_AS_ADVANCED(
  LASH_INCLUDE_DIR
  LASH_LIBRARIES
)
