#ifndef __APP_INFO_H
#define __APP_INFO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_launcher.h"

/* 系统信息 app：运行时长 + LVGL 内存占用，兼作新 app 的编写模板 */
extern const app_t app_info;

#ifdef __cplusplus
}
#endif

#endif /* __APP_INFO_H */
