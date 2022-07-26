#pragma once
/*
 * OMX IL Alsa Sink component
 * Copyright (c) 2016 Timo Teräs
 *
 * This Program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * TODO:
 * - timeouts for state transition failures
 */

#include <IL/OMX_Core.h>

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMXALSA_GetHandle(
    OMX_OUT OMX_HANDLETYPE* pHandle,
    OMX_IN  OMX_STRING cComponentName,
    OMX_IN  OMX_PTR pAppData,
    OMX_IN  OMX_CALLBACKTYPE* pCallBacks);

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMXALSA_FreeHandle(
    OMX_IN  OMX_HANDLETYPE hComponent);
