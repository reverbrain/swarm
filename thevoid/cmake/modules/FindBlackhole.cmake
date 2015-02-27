# Find Blackhole library
#
# This module defines
# Blackhole_FOUND - whether the Blackhole was found
# Blackhole_INCLUDE_DIRS - the include path of the Blackhole library

FIND_PATH (Blackhole_INCLUDE_DIR blackhole/blackhole.hpp)

INCLUDE (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS (Blackhole DEFAULT_MSG Blackhole_INCLUDE_DIR)
SET (Blackhole_FOUND BLACKHOLE_FOUND)

IF (Blackhole_FOUND)
	SET (Blackhole_INCLUDE_DIRS ${Blackhole_INCLUDE_DIR})
ELSE (Blackhole_FOUND)
	SET (Blackhole_INCLUDE_DIRS)
ENDIF (Blackhole_FOUND)
