# vim: ts=2 sw=2
# - Try to find the required ffmpeg Poco (default: Zip Net NetSSL Crypto Util XML Foundation)
#
# Once done this will define
#  Poco_FOUND         - System has the all required components.
#  Poco_INCLUDE_DIRS  - Include directory necessary for using the required components headers.
#  Poco_LIBRARY_DIRS  - Library directories necessary for using the required components.
#  Poco_LIBRARIES     - Link these to use the required components.
#  Poco_DEFINITIONS   - Compiler switches required for using the required components.
#
# For each of the components it will additionaly set.
#   - Foundation
#   - CppUnit
#   - Net
#   - NetSSL
#   - Crypto
#   - Util
#   - XML
#   - Zip
#   - Data
#   - PageCompiler
#
# the following variables will be defined
#  Poco_<component>_FOUND        - System has <component>
#  Poco_<component>_INCLUDE_DIRS - Include directories necessary for using the <component> headers
#  Poco_<component>_LIBRARY_DIRS - Library directories necessary for using the <component>
#  Poco_<component>_LIBRARIES    - Link these to use <component>
#  Poco_<component>_DEFINITIONS  - Compiler switches required for using <component>
#  Poco_<component>_VERSION      - The components version

# The default components to find
if (NOT Poco_FIND_COMPONENTS)
  set(Poco_FIND_COMPONENTS Foundation Net NetSSL Crypto Util Zip) # XML
endif()

# Link library type
set(Poco_LINK_SHARED_LIBS FALSE CACHE BOOL "Link with shared Poco libraries (.dll/.so) instead of static ones (.lib/.a)")    
  
# Check for cached results. If there are then skip the costly part.
set_module_notfound(Poco)
if (NOT Poco_FOUND)

  # Set the library suffix for our build type
  set(Poco_LIB_SUFFIX "")
  if(WIN32 AND MSVC)
    set(Poco_MULTI_CONFIGURATION TRUE)
    #add_definitions(-DPOCO_NO_AUTOMATIC_LIBS)
    #add_definitions(-DPOCO_NO_UNWINDOWS)
    if(Poco_LINK_SHARED_LIBS)
      add_definitions(-DPOCO_DLL)
    else()
      add_definitions(-DPOCO_STATIC)
      if(BUILD_WITH_STATIC_CRT)
        set(Poco_LIB_SUFFIX "mt")
      else()
        set(Poco_LIB_SUFFIX "md")
      endif()    
    endif()
  endif()

  # Set search path suffixes for windows
  if(WIN32)
    set(Poco_PATH_SUFFIXES
      /CppUnit/include
      /Data/include
      /Foundation/include
      /Crypto/include
      /Net/include
      /NetSSL_OpenSSL/include
      /PageCompiler/include
      /Util/include
      /XML/include
      /Zip/include
      )    
  endif()
      
  # Check for all possible components 
  find_component_paths(Poco Foundation   PocoFoundation${Poco_LIB_SUFFIX} Poco/Foundation.h)
  find_component_paths(Poco CppUnit      PocoCppUnit${Poco_LIB_SUFFIX}    Poco/CppUnit/CppUnit.h)
  find_component_paths(Poco Net          PocoNet${Poco_LIB_SUFFIX}        Poco/Net/Net.h)
  find_component_paths(Poco NetSSL       PocoNetSSL${Poco_LIB_SUFFIX}     Poco/Net/NetSSL.h)
  find_component_paths(Poco Crypto       PocoCrypto${Poco_LIB_SUFFIX}     Poco/Crypto/Crypto.h)
  find_component_paths(Poco Util         PocoUtil${Poco_LIB_SUFFIX}       Poco/Util/Util.h)
  find_component_paths(Poco XML          PocoXML${Poco_LIB_SUFFIX}        Poco/XML/XML.h)
  find_component_paths(Poco Zip          PocoZip${Poco_LIB_SUFFIX}        Poco/Zip/Zip.h)
  find_component_paths(Poco Data         PocoData${Poco_LIB_SUFFIX}       Poco/Data/Data.h)
  find_component_paths(Poco PageCompiler PocoPageCompiler                 Poco/PageCompiler/PageCompiler.h)  
   
  # Set Poco as found or not
  set_module_found(Poco)
endif ()



      # Now set the noncached _FOUND vars for the components.
      #Zip Net NetSSL Crypto Util XML Foundation
      #foreach (component Net NetSSL Crypto Util XML Foundation Zip Data CppUnit PageCompiler)
      #  set_component_notfound(Poco ${component})
      #endforeach ()






  # Cache the vars.
  #set(Poco_INCLUDE_DIRS ${Poco_INCLUDE_DIRS} CACHE STRING  "The Poco include directories." FORCE)
  #set(Poco_LIBRARIES    ${Poco_LIBRARIES}    CACHE STRING  "The Poco libraries." FORCE)
  #set(Poco_DEFINITIONS  ${Poco_DEFINITIONS}  CACHE STRING  "The Poco cflags." FORCE)  
  #set(Poco_FOUND        ${Poco_FOUND}        CACHE BOOLEAN "The Poco found status." FORCE)




#if (NOT Poco_FOUND)
#	message(ERROR "Cannot find Poco.")
#endif ()

    # Check that the required components were found.  
  #set(Poco_FOUND 1)
  #foreach (component ${Poco_FIND_COMPONENTS})
  #  if (POCO_${component}_FOUND)
  #    message(STATUS "Required component ${component} present.")
  #  else ()
  #    message(ERROR "Required component ${component} missing.")
  #    set(Poco_FOUND 0)
  #  endif()
  #endforeach()


  #mark_as_advanced(Poco_INCLUDE_DIRS
  #                 Poco_LIBRARIES
  #                 Poco_DEFINITIONS)

# Compile the list of required vars
#set(_Poco_REQUIRED_VARS Poco_LIBRARIES Poco_INCLUDE_DIRS)
#foreach (component ${Poco_FIND_COMPONENTS})
#  list(APPEND _Poco_REQUIRED_VARS Poco_${component}_LIBRARIES Poco_${component}_INCLUDE_DIRS)
#endforeach ()

# Give a nice error message if some of the required vars are missing.
#find_package_handle_standard_args(Poco DEFAULT_MSG ${_Poco_REQUIRED_VARS})






  
  # If we are on windows find_component will not be able to resolve the include paths
  # so we need to add them manually  
  
#  find_path(Poco_DIR 
#    NAMES 
#      Foundation/include/Poco/Foundation.h 
#    PATH_SUFFIXES 
#      ${Poco_DIR_SUFFIXES} 
#    PATHS
#      ${Poco_POSSIBLE_ROOT_DIRS}
#    DOC 
#      "The ${Poco_INCLUDE_PATH_DESCRIPTION}"
#  )

  # Build the include path with duplicates removed.
  #if (Poco_INCLUDE_DIRS)
  #  list(REMOVE_DUPLICATES Poco_INCLUDE_DIRS)
  #endif ()
  #if (Poco_LIBRARIES)
  #  list(REMOVE_DUPLICATES Poco_LIBRARIES)
  #endif ()
# Now set the noncached _FOUND vars for the components.
#Zip Net NetSSL Crypto Util XML Foundation
#foreach (component Net NetSSL Crypto Util XML Foundation Zip Data CppUnit PageCompiler)
#  setcomponent_found(${component})
#endforeach ()