

#
### Macro: set_component_found
#
# Marks the given component as found if both *_LIBRARIES AND *_INCLUDE_DIRS is present.
#
macro(set_component_found _component )
  if (${_component}_LIBRARIES AND ${_component}_INCLUDE_DIRS)
     # message(STATUS "  - ${_component} found.")
    set(${_component}_FOUND TRUE)
  else ()
     # message(STATUS "  - ${_component} not found.")
  endif ()
endmacro()

#
### Macro: find_component
#
# Checks for the given component by invoking pkgconfig and then looking up the libraries and
# include directories.
#
macro(find_component _component _pkgconfig _library _header)

  if (NOT WIN32)
     # use pkg-config to get the directories and then use these values
     # in the FIND_PATH() and FIND_LIBRARY() calls
     find_package(PkgConfig)
     if (PKG_CONFIG_FOUND)
       pkg_check_modules(PC_${_component} ${_pkgconfig})
     endif()
  endif()

  find_path(${_component}_INCLUDE_DIRS ${_header}
    HINTS
      ${PC_LIB${_component}_INCLUDEDIR}
      ${PC_LIB${_component}_INCLUDE_DIRS}
    #PATH_SUFFIXES
    #  ffmpeg
  )

  find_library(${_component}_LIBRARIES 
    NAMES # setting in order might help overcome find_library bugs :/
      lib${_library}.a 
      ${_library}.lib
      ${_library}
    HINTS
      ${PC_LIB${_component}_LIBDIR}
      ${PC_LIB${_component}_LIBRARY_DIRS}
    PATH_SUFFIXES 
      lib
      bin
  )
  
  #message("find_component=lib${_library}.a")
  #message("find_component=${${_component}_INCLUDE_DIRS}")
  #message("find_component_INCLUDE_DIRS=${${_component}_INCLUDE_DIRS}")
  #message("find_component_LIBRARIES=${${_component}_LIBRARIES}")
  #message("${${_component}_INCLUDE_DIRS}/lib")
  #message("${PC_LIB${_component}_LIBDIR}")
  #message("${PC_LIB${_component}_LIBRARY_DIRS}")
  #message("${_header}")
  #message("${_library}")

  set(${_component}_DEFINITIONS  ${PC_${_component}_CFLAGS_OTHER} CACHE STRING "The ${_component} CFLAGS.")
  set(${_component}_VERSION      ${PC_${_component}_VERSION}      CACHE STRING "The ${_component} version number.")

  set_component_found(${_component})

  mark_as_advanced(
    ${_component}_INCLUDE_DIRS
    ${_component}_LIBRARIES
    ${_component}_DEFINITIONS
    ${_component}_VERSION)

endmacro()