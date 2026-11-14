//========================================================================
// PLFW 3.5 POSIX - windowing-only fork of GLFW (www.glfw.org)
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

#if defined(PLFW_BUILD_POSIX_THREAD)

#include <assert.h>
#include <string.h>


//////////////////////////////////////////////////////////////////////////
//////                       PLFW platform API                      //////
//////////////////////////////////////////////////////////////////////////

PLFWbool _plfwPlatformCreateTls(_PLFWtls* tls)
{
    assert(tls->posix.allocated == PLFW_FALSE);

    if (pthread_key_create(&tls->posix.key, NULL) != 0)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "POSIX: Failed to create context TLS");
        return PLFW_FALSE;
    }

    tls->posix.allocated = PLFW_TRUE;
    return PLFW_TRUE;
}

void _plfwPlatformDestroyTls(_PLFWtls* tls)
{
    if (tls->posix.allocated)
        pthread_key_delete(tls->posix.key);
    memset(tls, 0, sizeof(_PLFWtls));
}

void* _plfwPlatformGetTls(_PLFWtls* tls)
{
    assert(tls->posix.allocated == PLFW_TRUE);
    return pthread_getspecific(tls->posix.key);
}

void _plfwPlatformSetTls(_PLFWtls* tls, void* value)
{
    assert(tls->posix.allocated == PLFW_TRUE);
    pthread_setspecific(tls->posix.key, value);
}

PLFWbool _plfwPlatformCreateMutex(_PLFWmutex* mutex)
{
    assert(mutex->posix.allocated == PLFW_FALSE);

    if (pthread_mutex_init(&mutex->posix.handle, NULL) != 0)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR, "POSIX: Failed to create mutex");
        return PLFW_FALSE;
    }

    return mutex->posix.allocated = PLFW_TRUE;
}

void _plfwPlatformDestroyMutex(_PLFWmutex* mutex)
{
    if (mutex->posix.allocated)
        pthread_mutex_destroy(&mutex->posix.handle);
    memset(mutex, 0, sizeof(_PLFWmutex));
}

void _plfwPlatformLockMutex(_PLFWmutex* mutex)
{
    assert(mutex->posix.allocated == PLFW_TRUE);
    pthread_mutex_lock(&mutex->posix.handle);
}

void _plfwPlatformUnlockMutex(_PLFWmutex* mutex)
{
    assert(mutex->posix.allocated == PLFW_TRUE);
    pthread_mutex_unlock(&mutex->posix.handle);
}

#endif // PLFW_BUILD_POSIX_THREAD

