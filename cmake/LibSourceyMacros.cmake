#
### Macro: subdirlist
#
# Returns a list of subdirectories.
#
macro(subdirlist result curdir)
  file(GLOB children RELATIVE ${curdir} ${curdir}/*)
  set(dirlist "")
  foreach(child ${children})
    if(IS_DIRECTORY ${curdir}/${child})
        set(dirlist ${dirlist} ${child})
    endif()
  endforeach()
  set(${result} ${dirlist})
endmacro()


#
### Macro: join
#
# Joins a string array.
# Example:
#   SET( letters "" "\;a" b c "d\;d" )
#   JOIN("${letters}" ":" output)
#   MESSAGE("${output}") # :;a:b:c:d;d
#
function(JOIN VALUES GLUE OUTPUT)
  string (REGEX REPLACE "([^\\]|^);" "\\1${GLUE}" _TMP_STR "${VALUES}")
  string (REGEX REPLACE "[\\](.)" "\\1" _TMP_STR "${_TMP_STR}") #fixes escaping
  set (${OUTPUT} "${_TMP_STR}" PARENT_SCOPE)
endfunction()


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
### Macro: set_component_notfound
#
# Marks the given component as not found, and resets the cache for find_path and find_library results.
#
macro(set_component_notfound _component)

  # message(STATUS "  - ${_component} setting notfound.")
  set(${_component}_FOUND FALSE)
  
  if (CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE)  
    set(${_component}_RELEASE_LIBRARIES ${_component}_RELEASE_LIBRARIES-NOTFOUND)
    set(${_component}_DEBUG_LIBRARIES ${_component}_DEBUG_LIBRARIES-NOTFOUND)
    set(${_component}_LIBRARIES "") #${_component}_LIBRARIES-NOTFOUND)
  else()
    set(${_component}_LIBRARIES ${_component}_LIBRARIES-NOTFOUND)
  endif()  
    
endmacro()


#
### Macro: find_component
#
# Checks for the given component by invoking pkgconfig and then looking up the libraries and
# include directories.
#
macro(find_component _module _component _pkgconfig _library _header)

  if (NOT WIN32)
     # use pkg-config to get the directories and then use these values
     # in the FIND_PATH() and FIND_LIBRARY() calls
     find_package(PkgConfig)
     if (PKG_CONFIG_FOUND)
        set(ALIAS               ALIASOF_${_pkgconfig})
        set(ALIAS_FOUND                 ${ALIAS}_FOUND)
        set(ALIAS_INCLUDE_DIRS   ${ALIAS}_INCLUDE_DIRS)
        set(ALIAS_LIBRARY_DIRS   ${ALIAS}_LIBRARY_DIRS)
        set(ALIAS_LIBRARIES         ${ALIAS}_LIBRARIES)

        #pkg_check_modules(PC_${_component} ${_pkgconfig})
        PKG_CHECK_MODULES(${ALIAS} ${_pkgconfig})

        if(${ALIAS_FOUND})
          #set(${define} 1)
          foreach(P "${ALIAS_INCLUDE_DIRS}")
            if(${P})
              list(APPEND LibSourcey_INCLUDE_DIRS ${${P}})
              #message("ALIAS_INCLUDE_DIRS=${${P}}")
            endif()
          endforeach()

          foreach(P "${ALIAS_LIBRARY_DIRS}")
            if(${P})
              list(APPEND LibSourcey_LIBRARY_DIRS${${P}})
            endif()
          endforeach()

          list(APPEND LibSourcey_INCLUDE_LIBRARIES ${${ALIAS_LIBRARIES}})             
	   #message("ALIAS_LIBRARIES=${${ALIAS_LIBRARIES}}")
        endif()
     endif()
  endif()
  
  message("ALIAS_LIBRARY_DIRS=${${ALIAS_INCLUDE_DIRS}}")

  find_path(${_component}_INCLUDE_DIRS ${_header}
    HINTS
      ${ALIAS_INCLUDE_DIRS}
	#${PC_LIB${_component}_INCLUDEDIR}
       #${PC_LIB${_component}_INCLUDE_DIRS}
    #PATH_SUFFIXES
    #  ${{_module}_PATH_SUFFIXES}
    #  ffmpeg
  )

  find_library(${_component}_LIBRARIES 
    NAMES # setting in order might help overcome find_library bugs :/
      lib${_library}.a 
      ${_library}.lib
      ${_library}
    HINTS
      ${ALIAS_INCLUDE_DIRS}
      #${PC_LIB${_component}_LIBDIR}
      #${PC_LIB${_component}_LIBRARY_DIRS}
    PATH_SUFFIXES 
      lib
      bin
  )        
  
  #message("find_component_PATH_SUFFIXES=${{_module}_PATH_SUFFIXES}")
  #message("find_component_INCLUDE_DIRS=${${_component}_INCLUDE_DIRS}")
  #message("find_component=lib${_library}.a")
  #message("find_component=${${_component}_INCLUDE_DIRS}")
  #message("find_component_LIBRARIES=${${_component}_LIBRARIES}")  
  #message("find_component_PC=${PC_${_component}}")  
  #message("find_component_PCg=${_pkgconfig}")
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
    ${_component}_DEBUG_LIBRARIES
    ${_component}_RELEASE_LIBRARIES
    ${_component}_LIBRARIES
    ${_component}_DEFINITIONS
    ${_component}_VERSION)

endmacro()

#
### Macro: find_multi_component
#
# Checks for the given multi configuration component by invoking pkgconfig and then looking up the
# libraries and include directories.
# Extra helper variables may be set to assist finding libraries:
#   ${_module}_PATH_SUFFIXES
#
macro(find_multi_component _module _component _pkgconfig _library _header)

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
    PATH_SUFFIXES
      ${${_module}_PATH_SUFFIXES}
    #  ffmpeg
  )

  # Create a Debug and a Release list for multi configuration builds        
  if (CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE)
    #set(${_component}_RELEASE_LIBRARIES ${_component}_RELEASE_LIBRARIES-NOTFOUND)
    #set(${_component}_DEBUG_LIBRARIES ${_component}_DEBUG_LIBRARIES-NOTFOUND)
    #set(${_component}_LIBRARIES)
    find_library(${_component}_RELEASE_LIBRARIES 
      NAMES
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
    find_library(${_component}_DEBUG_LIBRARIES 
      NAMES
        lib${_library}d.a 
        ${_library}d.lib
        ${_library}d
      HINTS
        ${PC_LIB${_component}_LIBDIR}
        ${PC_LIB${_component}_LIBRARY_DIRS}
      PATH_SUFFIXES 
        lib
        bin
    )     
    if (${_component}_RELEASE_LIBRARIES) 
      list(APPEND ${_component}_LIBRARIES "optimized" ${${_component}_RELEASE_LIBRARIES})
    endif()
    if (${_component}_DEBUG_LIBRARIES)
      list(APPEND ${_component}_LIBRARIES "debug" ${${_component}_DEBUG_LIBRARIES})
    endif()  
    
    #message("R-----${_component}=${${_component}_LIBRARIES}")
    #message("D-----${_component}=${${_component}_LIBRARIES}")
  else()
    #set(${_component}_LIBRARIES)
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
  endif()        
  
  #message("find_component=Searching for: ${_library}")
  #message("find_component_module=${${_module}_PATH_SUFFIXES}")
  #message("find_component_PATH_SUFFIXES=${${_module}_PATH_SUFFIXES}")
  #message("find_component_INCLUDE_DIRS=${${_component}_INCLUDE_DIRS}")
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
    ${_component}_DEBUG_LIBRARIES
    ${_component}_RELEASE_LIBRARIES
    ${_component}_DEFINITIONS
    ${_component}_VERSION)

endmacro()




#    # Create a Debug and a Release list for multi configuration builds        
#  if (CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE)
#    find_library(${_component}_RELEASE_LIBRARIES 
#      NAMES
#        lib${_library}.a 
#        ${_library}.lib
#        ${_library}
#      HINTS
#        ${PC_LIB${_component}_LIBDIR}
#        ${PC_LIB${_component}_LIBRARY_DIRS}
#      PATH_SUFFIXES 
#        lib
#        bin
#    ) 
#    find_library(${_component}_DEBUG_LIBRARIES 
#      NAMES
#        lib${_library}d.a 
#        ${_library}d.lib
#        ${_library}d
#      HINTS
#        ${PC_LIB${_component}_LIBDIR}
#        ${PC_LIB${_component}_LIBRARY_DIRS}
#      PATH_SUFFIXES 
#        lib
#        bin
#    ) 
#    #message("R-----${${_component}_RELEASE_LIBRARIES}=${${_component}_LIBRARIES}")
#    #message("D-----${${_component}_DEBUG_LIBRARIES}=${${_component}_LIBRARIES}")
#    
#    if (${_component}_RELEASE_LIBRARIES) 
#      list(APPEND ${_component}_LIBRARIES "optimized" ${${_component}_RELEASE_LIBRARIES})
#    endif()
#    if (${_component}_DEBUG_LIBRARIES)
#      list(APPEND ${_component}_LIBRARIES "debug" ${${_component}_DEBUG_LIBRARIES})
#    endif()  
#  else()
#    find_library(${_component}_LIBRARIES 
#      NAMES # setting in order might help overcome find_library bugs :/
#        lib${_library}.a 
#        ${_library}.lib
#        ${_library}
#      HINTS
#        ${PC_LIB${_component}_LIBDIR}
#        ${PC_LIB${_component}_LIBRARY_DIRS}
#      PATH_SUFFIXES 
#        lib
#        bin
#    )  
#  endif()  
  