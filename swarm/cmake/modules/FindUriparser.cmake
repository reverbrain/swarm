# Find Uriparser library
#
# This module defines
# Uriparser_FOUND - whether the Uriparser was found
# Uriparser_LIBRARIES - Uriparser libraries
# Uriparser_INCLUDE_DIRS - the include path of the Uriparser library

FIND_PATH (Uriparser_INCLUDE_DIR uriparser/Uri.h)
FIND_LIBRARY (Uriparser_LIBRARY uriparser)

INCLUDE (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS (Uriparser DEFAULT_MSG Uriparser_LIBRARY Uriparser_INCLUDE_DIR)
SET (Uriparser_FOUND URIPARSER_FOUND)

IF (Uriparser_FOUND)
	SET (Uriparser_LIBRARIES ${Uriparser_LIBRARY})
	SET (Uriparser_INCLUDE_DIRS ${Uriparser_INCLUDE_DIR})
ELSE (Uriparser_FOUND)
	SET (Uriparser_LIBRARIES)
	SET (Uriparser_INCLUDE_DIRS)
ENDIF (Uriparser_FOUND)
