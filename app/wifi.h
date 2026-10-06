#ifndef __WIFI_H__
#define __WIFI_H__

#include <stdbool.h>

#define APP_VERSION "v1.0"


bool wifi_init(void);
bool wifi_wait_connect(void);


#endif /* __WIFI_H__ */
