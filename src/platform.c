//========================================================================
// PLFW 3.5 - windowing-only fork of GLFW (www.glfw.org)
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2018 Camilla Löwy <elmindreda@glfw.org>
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

#include <string.h>
#include <stdlib.h>

//////////////////////////////////////////////////////////////////////////
//////                       PLFW internal API                      //////
//////////////////////////////////////////////////////////////////////////

static const struct
{
    int ID;
    PLFWbool (*connect)(int,_PLFWplatform*);
} supportedPlatforms[] =
{
#if defined(_PLFW_WIN32)
    { PLFW_PLATFORM_WIN32, _plfwConnectWin32 },
#endif
#if defined(_PLFW_WAYLAND)
    { PLFW_PLATFORM_WAYLAND, _plfwConnectWayland },
#endif
};

PLFWbool _plfwSelectPlatform(int desiredID, _PLFWplatform* platform)
{
    const size_t count = sizeof(supportedPlatforms) / sizeof(supportedPlatforms[0]);
    size_t i;

    if (desiredID != PLFW_ANY_PLATFORM &&
        desiredID != PLFW_PLATFORM_WIN32 &&
        desiredID != PLFW_PLATFORM_WAYLAND &&
        desiredID != PLFW_PLATFORM_NULL)
    {
        _plfwInputError(PLFW_INVALID_ENUM, "Invalid platform ID 0x%08X", desiredID);
        return PLFW_FALSE;
    }

    // Only allow the Null platform if specifically requested
    if (desiredID == PLFW_PLATFORM_NULL)
        return _plfwConnectNull(desiredID, platform);
    else if (count == 0)
    {
        _plfwInputError(PLFW_PLATFORM_UNAVAILABLE, "This binary only supports the Null platform");
        return PLFW_FALSE;
    }

    if (desiredID == PLFW_ANY_PLATFORM)
    {
        // If there is exactly one platform available for auto-selection, let it emit the
        // error on failure as the platform-specific error description may be more helpful
        if (count == 1)
            return supportedPlatforms[0].connect(supportedPlatforms[0].ID, platform);

        for (i = 0;  i < count;  i++)
        {
            if (supportedPlatforms[i].connect(desiredID, platform))
                return PLFW_TRUE;
        }

        _plfwInputError(PLFW_PLATFORM_UNAVAILABLE, "Failed to detect any supported platform");
    }
    else
    {
        for (i = 0;  i < count;  i++)
        {
            if (supportedPlatforms[i].ID == desiredID)
                return supportedPlatforms[i].connect(desiredID, platform);
        }

        _plfwInputError(PLFW_PLATFORM_UNAVAILABLE, "The requested platform is not supported");
    }

    return PLFW_FALSE;
}

//////////////////////////////////////////////////////////////////////////
//////                        PLFW public API                       //////
//////////////////////////////////////////////////////////////////////////

PLFWAPI int plfwGetPlatform(void)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(0);
    return _plfw.platform.platformID;
}

PLFWAPI int plfwPlatformSupported(int platformID)
{
    const size_t count = sizeof(supportedPlatforms) / sizeof(supportedPlatforms[0]);
    size_t i;

    if (platformID != PLFW_PLATFORM_WIN32 &&
        platformID != PLFW_PLATFORM_WAYLAND &&
        platformID != PLFW_PLATFORM_NULL)
    {
        _plfwInputError(PLFW_INVALID_ENUM, "Invalid platform ID 0x%08X", platformID);
        return PLFW_FALSE;
    }

    if (platformID == PLFW_PLATFORM_NULL)
        return PLFW_TRUE;

    for (i = 0;  i < count;  i++)
    {
        if (platformID == supportedPlatforms[i].ID)
            return PLFW_TRUE;
    }

    return PLFW_FALSE;
}

