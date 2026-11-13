//========================================================================
// PLFW 3.5 Win32 - windowing-only fork of GLFW (www.glfw.org)
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2017 Camilla Löwy <elmindreda@glfw.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#include "internal.h"

#if defined(PLFW_BUILD_WIN32_THREAD)

#include <assert.h>


//////////////////////////////////////////////////////////////////////////
//////                       PLFW platform API                      //////
//////////////////////////////////////////////////////////////////////////

PLFWbool _plfwPlatformCreateTls(_PLFWtls* tls)
{
    assert(tls->win32.allocated == PLFW_FALSE);

    tls->win32.index = TlsAlloc();
    if (tls->win32.index == TLS_OUT_OF_INDEXES)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR, "Win32: Failed to allocate TLS index");
        return PLFW_FALSE;
    }

    tls->win32.allocated = PLFW_TRUE;
    return PLFW_TRUE;
}

void _plfwPlatformDestroyTls(_PLFWtls* tls)
{
    if (tls->win32.allocated)
        TlsFree(tls->win32.index);
    memset(tls, 0, sizeof(_PLFWtls));
}

void* _plfwPlatformGetTls(_PLFWtls* tls)
{
    assert(tls->win32.allocated == PLFW_TRUE);
    return TlsGetValue(tls->win32.index);
}

void _plfwPlatformSetTls(_PLFWtls* tls, void* value)
{
    assert(tls->win32.allocated == PLFW_TRUE);
    TlsSetValue(tls->win32.index, value);
}

PLFWbool _plfwPlatformCreateMutex(_PLFWmutex* mutex)
{
    assert(mutex->win32.allocated == PLFW_FALSE);
    InitializeCriticalSection(&mutex->win32.section);
    return mutex->win32.allocated = PLFW_TRUE;
}

void _plfwPlatformDestroyMutex(_PLFWmutex* mutex)
{
    if (mutex->win32.allocated)
        DeleteCriticalSection(&mutex->win32.section);
    memset(mutex, 0, sizeof(_PLFWmutex));
}

void _plfwPlatformLockMutex(_PLFWmutex* mutex)
{
    assert(mutex->win32.allocated == PLFW_TRUE);
    EnterCriticalSection(&mutex->win32.section);
}

void _plfwPlatformUnlockMutex(_PLFWmutex* mutex)
{
    assert(mutex->win32.allocated == PLFW_TRUE);
    LeaveCriticalSection(&mutex->win32.section);
}

#endif // PLFW_BUILD_WIN32_THREAD

