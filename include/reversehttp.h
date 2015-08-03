/**********************************************************
Description: Reversehttp for C client, detail:http://reversehttp.net/
Author: ddxu
Mail: ddxu@caton.com.cn
Last-Modify-Date: 2014-03-21
Coypright: Caton Technology ©2014 All rights reserved
**********************************************************/
#ifndef __REVERSEHTTP_H__
#define __REVERSEHTTP_H__
#include <curl/curl.h>

struct reversehttp_s
{
	/* curl handles */
	CURL *remote_curl_handle;
	CURL *local_curl_handle;
	
    /* params */
    char *label;
    char *endpoint;
    char *localhost_url;

    char *next_request_url;
    char *next_request_url_tmp;
    char *repost_request_header;
    char *location;
	int need_repost_request;
    /* callback funcitons */
    void (*on_location_changed)(struct reversehttp_s *reversehttp);
};

/* ----------------------------- function declare ------------------------------*/
int http_redirect(struct reversehttp_s *reversehttp, const char *request);
int http_get(struct reversehttp_s *reversehttp, char *url, struct curl_slist *headers);
int http_post(struct reversehttp_s *reversehttp,char *url,void *body, int len, struct curl_slist *custom_headers);
int http_get_loop(struct reversehttp_s *reversehttp);
int init_curl_handles(struct reversehttp_s *reversehttp);

int routine(struct reversehttp_s *reversehttp );

int reversehttp_get(struct reversehttp_s *reversehttp, 
				    CURL *curl,
					char *url);

int reversehttp_post(struct reversehttp_s *reversehttp,
					CURL *curl,
					char *url,
					void *body, 
					size_t len, 
					struct curl_slist *custom_headers);
					
int reversehttp_redirect(struct reversehttp_s *reversehttp, 
						CURL *curl,
						char *url,
						const char *request_header,
						unsigned char **result,
						size_t *result_len);

#endif
