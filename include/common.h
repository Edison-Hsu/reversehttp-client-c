/**********************************************************
Description: Reversehttp for C client, detail:http://reversehttp.net/
Author: ddxu
Mail: ddxu@caton.com.cn
Last-Modify-Date: 2014-03-21
Coypright: Caton Technology ©2014 All rights reserved
**********************************************************/
#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <syslog.h>

/* ------------------------------debug defines------------------------------ */
#define PRINT_ERR(fmt,args...)  syslog(LOG_ERR, "[ERROR: %s:%s:%d]: "fmt":%s\n", \
                            __FILE__, __FUNCTION__, __LINE__, ##args, strerror(errno))
#define FP_INFO(fmt,args...) syslog(LOG_INFO,"[INFO: %s:%s:%d]: " fmt, __FILE__, __FUNCTION__,  __LINE__,##args)

#ifdef __DEBUG__
#define PRINT_DEBUG(fmt,args...) syslog(LOG_DEBUG,"[DEBUG: %s:%s:%d]: " fmt, __FILE__, __FUNCTION__,  __LINE__,##args)
#else
#define PRINT_DEBUG(fmt,args...) 
#endif

#endif

