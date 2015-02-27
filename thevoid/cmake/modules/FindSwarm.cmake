# Find Swarm library
#
# This module defines
# Swarm_FOUND - whether the Swarm was found
# Swarm_LIBRARIES - Swarm libraries
# Swarm_INCLUDE_DIRS - the include path of the Swarm library

FIND_PATH (Swarm_INCLUDE_DIR swarm/http_headers.hpp)
FIND_LIBRARY (Swarm_LIBRARY swarm)

INCLUDE (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS (Swarm DEFAULT_MSG Swarm_LIBRARY Swarm_INCLUDE_DIR)
SET (Swarm_FOUND SWARM_FOUND)

IF (Swarm_FOUND)
	SET (Swarm_LIBRARIES ${Swarm_LIBRARY})
	SET (Swarm_INCLUDE_DIRS ${Swarm_INCLUDE_DIR})
ELSE (Swarm_FOUND)
	SET (Swarm_LIBRARIES)
	SET (Swarm_INCLUDE_DIRS)
ENDIF (Swarm_FOUND)
