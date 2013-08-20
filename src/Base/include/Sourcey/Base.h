//
// LibSourcey
// Copyright (C) 2005, Sourcey <http://sourcey.com>
//
// LibSourcey is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// LibSourcey is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//


#ifndef SOURCEY_H
#define SOURCEY_H


//
/// Version number  
//

#define SOURCEY_MAJOR_VERSION    0
#define SOURCEY_MINOR_VERSION    9
#define SOURCEY_PATCH_VERSION    0

#define SOURCEY_AUX_STR_EXP(__A) #__A
#define SOURCEY_AUX_STR(__A)     SOURCEY_AUX_STR_EXP(__A)
#define SOURCEY_VERSION          SOURCEY_AUX_STR(SOURCEY_MAJOR_VERSION) "." SOURCEY_AUX_STR(SOURCEY_MINOR_VERSION) "." SOURCEY_AUX_STR(SOURCEY_PATCH_VERSION)


//
/// Cross platform configuration
//

#include "LibSourceyConfig.h"

#ifdef _WIN32
    // Windows (x64 and x86)
	#ifndef WIN32
    #define WIN32 //1
	#endif
#endif
#if __unix__ // all unices
    // Unix    
    #define UNIX
#endif
#if __posix__
    // POSIX    
    #define POSIX
#endif
#if __linux__
    // linux    
    #define LINUX
#endif
#if __GNUC__
    // GCC specific
    #define GNUC
#endif
#if __APPLE__
    // Mac OS
    #define APPLE
#endif
	

#ifdef WIN32
# ifndef SOURCEY_SHARED_LIBRARY
#   define SOURCEY_EXTERN __declspec(dllexport)
# else
#   define SOURCEY_EXTERN __declspec(dllimport)
# endif
#else
# define SOURCEY_EXTERN // nothing
#endif


//
/// Windows specific
//

#ifdef WIN32

// Verify that we're built with the multithreaded 
// versions of the runtime libraries
#if defined(_MSC_VER) && !defined(_MT)
	#error Must compile with /MD, /MDd, /MT or /MTd
#endif


// Check debug/release settings consistency
#if defined(NDEBUG) && defined(_DEBUG)
	#error Inconsistent build settings (check for /MD[d])
#endif


// Unicode Support
#if defined(UNICODE)
	#define SOURCEY_UNICODE
#endif


// Disable unnecessary warnings
#if defined(_MSC_VER)
	#pragma warning(disable:4201) // nonstandard extension used : nameless struct/union
	#pragma warning(disable:4251) // ... needs to have dll-interface warning 
	#pragma warning(disable:4355) // 'this' : used in base member initializer list
	#pragma warning(disable:4996) // VC++ 8.0 deprecation warnings
	#pragma warning(disable:4351) // new behavior: elements of array '...' will be default initialized
	#pragma warning(disable:4675) // resolved overload was found by argument-dependent lookup
	#pragma warning(disable:4275) // non dll-interface class 'std::exception' used as base for dll-interface class 'scy::Exception'
#endif

#endif // WIN32


#endif // SOURCEY_H
