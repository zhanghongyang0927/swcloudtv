#ifndef __SWCLOUDTV_PRIV_H__
#define __SWCLOUDTV_PRIV_H__

#include "swlog.h"

#define CLOUDTV_LOG_DEBUG( format, ...) 	    sw_log( LOG_LEVEL_DEBUG, "swcloudtv", __FILE__, __LINE__, format, ##__VA_ARGS__  )
#define CLOUDTV_LOG_INFO( format, ... ) 	    sw_log( LOG_LEVEL_INFO, "swcloudtv", __FILE__, __LINE__, format, ##__VA_ARGS__  )
#define CLOUDTV_LOG_WARN( format, ... ) 	    sw_log( LOG_LEVEL_WARN, "swcloudtv", __FILE__, __LINE__, format, ##__VA_ARGS__  )
#define CLOUDTV_LOG_ERROR( format, ... ) 		sw_log( LOG_LEVEL_ERROR, "swcloudtv", __FILE__, __LINE__, format, ##__VA_ARGS__  )
#define CLOUDTV_LOG_FATAL( format, ... ) 		sw_log( LOG_LEVEL_FATAL, "swcloudtv", __FILE__, __LINE__, format, ##__VA_ARGS__  )

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif 


