# - Try to find the required LibSourcey components (default: base uv crypto net util)
#
# Once done this will define
#  LibSourcey_FOUND         - System has the all required components.
#  LibSourcey_INCLUDE_DIRS  - Include directory necessary for using the required components headers.
#  LibSourcey_LIBRARY_DIRS  - Library directories necessary for using the required components.
#  LibSourcey_LIBRARIES     - Link these to use the required components.
#  LibSourcey_DEFINITIONS   - Compiler switches required for using the required components.
#
# For each of the components it will additionally set.
#   - base
#   - CppParser
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
#  LibSourcey_<component>_FOUND        - System has <component>
#  LibSourcey_<component>_INCLUDE_DIRS - Include directories necessary for using the <component> headers
#  LibSourcey_<component>_LIBRARY_DIRS - Library directories necessary for using the <component>
#  LibSourcey_<component>_LIBRARIES    - Link these to use <component>
#  LibSourcey_<component>_DEFINITIONS  - Compiler switches required for using <component>
#  LibSourcey_<component>_VERSION      - The components version

# Set required variables
set(LibSourcey_ROOT_DIR "" CACHE STRING "Where is the LibSourcey root directory located?")
set(LibSourcey_INCLUDE_DIR "${LibSourcey_ROOT_DIR}/src" CACHE STRING "Where are the LibSourcey headers (.h) located?")
set(LibSourcey_LIBRARY_DIR "${LibSourcey_ROOT_DIR}/build/src" CACHE STRING "Where are the LibSourcey libraries (.dll/.so) located?")

# Include the LibSourcey cmake helpers (if included from third party library)
# if (LibSourcey_ROOT_DIR AND EXISTS ${LibSourcey_ROOT_DIR})
#   set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${LibSourcey_ROOT_DIR}/cmake)
#   set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} ${LibSourcey_ROOT_DIR}/cmake)
#
#   include(CMakeHelpers REQUIRED)
#   include(CMakeFindExtensions REQUIRED)
# endif()

include(CMakeHelpers REQUIRED)
include(CMakeFindExtensions REQUIRED)

# The default components to find
if (NOT LibSourcey_FIND_COMPONENTS)
  set(LibSourcey_FIND_COMPONENTS base uv crypto net util)
endif()

# Set a list of all available modules
set(LibSourcey_ALL_MODULES
  archo
  av
  base
  crypto
  http
  json
  net
  pacm
  pluga
  sked
  socketio
  stun
  symple
  turn
  util
  uv
  webrtc
)

# Check for cached results. If there are then skip the costly part.
# set_module_notfound(LibSourcey)
if (NOT LibSourcey_FOUND)

  # Set search path suffixes
  foreach(component ${LibSourcey_FIND_COMPONENTS})
    list(APPEND LibSourcey_INCLUDE_SUFFIXES ${component}/include)
    list(APPEND LibSourcey_LIBRARY_SUFFIXES ${component})
  endforeach()

  # Check for all available components
  find_component(LibSourcey archo    archo    scy_archo    scy/archo/zipfile.h)
  find_component(LibSourcey av       av       scy_av       scy/av/ffmpeg.h)
  find_component(LibSourcey base     base     scy_base     scy/base.h)
  find_component(LibSourcey crypto   crypto   scy_crypto   scy/crypto/crypto.h)
  find_component(LibSourcey http     http     scy_http     scy/http/server.h)
  find_component(LibSourcey json     json     scy_json     scy/json/json.h)
  find_component(LibSourcey net      net      scy_net      scy/net/socket.h)
  find_component(LibSourcey pacm     pacm     scy_pacm     scy/pacm/config.h)
  find_component(LibSourcey pluga    pluga    scy_pluga    scy/pluga/config.h)
  find_component(LibSourcey sked     sked     scy_sked     scy/sked/scheduler.h)
  find_component(LibSourcey socketio socketio scy_socketio scy/socketio/client.h)
  find_component(LibSourcey stun     stun     scy_stun     scy/stun/stun.h)
  find_component(LibSourcey symple   symple   scy_symple   scy/symple/client.h)
  find_component(LibSourcey turn     turn     scy_turn     scy/turn/turn.h)
  find_component(LibSourcey util     util     scy_util     scy/util/ratelimiter.h)
  find_component(LibSourcey uv       uv       scy_uv       scy/uv/uvpp.h)
  find_component(LibSourcey webrtc   webrtc   scy_webrtc   scy/webrtc/webrtc.h)

  # Set LibSourcey as found or not
  # print_module_variables(LibSourcey)
  set_module_found(LibSourcey)
endif()
