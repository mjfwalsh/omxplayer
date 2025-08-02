#pragma once

#if LIBAVCODEC_VERSION_MAJOR < 59
    #define AVCONST
#else
    #define AVCONST const
#endif
