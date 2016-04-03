/**********************************************************
Description: Reversehttp for C client, detail:http://reversehttp.net/
Author: EdisonHsu
Mail: 263113565@qq.com
Last-Modify-Date: 2014-03-21
**********************************************************/
#include "common.h"
#include "reversehttp.h"


/* ------------------------------static function declare ------------------------------*/
static int wait_on_socket(curl_socket_t sockfd, int for_recv, long timeout_ms);
static size_t capture_header( void *ptr, size_t size, size_t nmemb, void *userdata);
static size_t capture_body( void *ptr, size_t size, size_t nmemb, void *userdata);
static char * sub_strdup(const char *src, const char *start_flag, const char *end_flag);
static int parse_link_headers(struct reversehttp_s *reversehttp, const char *link_header);

/* ------------------------------implementations ------------------------------ */


/* use strdup, remember free */
static char * sub_strdup(const char *src, const char *start_flag, const char *end_flag)
{
    assert(src && start_flag && end_flag);
    char * result = NULL, *startp = NULL, *endp = NULL;
    
    do {
        startp = strstr(src, start_flag);
        if (NULL == startp)
            break;
        endp = strstr(startp+strlen(start_flag),end_flag);
        if (NULL == endp)
            break;

        result = strndup(startp+strlen(start_flag),
            endp-startp+1-strlen(start_flag)-strlen(end_flag));
    }while(0);
    
    return result;
}

int parse_link_headers(struct reversehttp_s *reversehttp, const char *link_header)
{
    assert(link_header && reversehttp);

    char *link = NULL;
    /* copy to my space in order to rewrite, ##remember free */
    link = strdup(link_header);
    if (NULL == link) {
        PRINT_ERR("strdup header");
        return -1;
    }

    char *phrase = NULL, *brkt = NULL, *sep=",";
    const char *url_sep_start = "<";
    const char *url_sep_end = ">";
    const char *rel_sep_start = "\"";
    const char *rel_sep_end = "\"";
    char *url = NULL, *rel = NULL;

    /* subsection Link option by comma */
    for (phrase = strtok_r(link, sep, &brkt); phrase; phrase = strtok_r(NULL, sep, &brkt)) {
        // PRINT_DEBUG("phrase = %s\n",phrase);
        /* token link url */
        if ( NULL != (url = sub_strdup(phrase, url_sep_start, url_sep_end)) ) {
            // PRINT_DEBUG("url:%s\n",url);
            /* token rel name */
            rel = sub_strdup(strstr(phrase,"rel="),rel_sep_start,rel_sep_end);
            if (rel != NULL) {
                // PRINT_DEBUG("rel:%s\n",rel);
                /* rel="first" or rel="next" means the url is next request */
                if( strcmp(rel,"first") == 0) {
                    /* get first request url */
                    reversehttp->next_request_url = url;
                    /* next_request_url_tmp copy form next_request_url */
                    reversehttp->next_request_url_tmp = strdup(url);
                } else if ((strcmp(rel, "next") == 0)) {
                    /* cache next_request_url_tmp if rel="next" */
                    /* when have user request from server, update next_request_url from next_request_url_tmp */
                    if (reversehttp->next_request_url_tmp != NULL) {
                        free(reversehttp->next_request_url_tmp);
                    }
                    reversehttp->next_request_url_tmp = url;
                    //printf("%s:%d:%s\n",__FUNCTION__,__LINE__,reversehttp->next_request_url_tmp);
                }else if (strcmp(rel,"related") == 0) {
                    /* rel="related" means the url is actual location */
                    reversehttp->location = url;
                    /* on_location_changed callback */
                    if(reversehttp->on_location_changed != NULL)
                        reversehttp->on_location_changed(reversehttp);
                }else {
                    PRINT_ERR("Error rel.");
                }

                /* free rel */
                if(rel)
                    free(rel);
            }

        }
    }

    if(link) {
        free(link);
        link = NULL;
    }

    return 0;
}

static size_t capture_header( void *ptr, size_t size, size_t nmemb, void *userdata)
{   
    char *header = (char *)ptr;
    struct reversehttp_s *reversehttp = (struct reversehttp_s *)userdata;
    //static int start_save_http_request = 0;
    const char *HEADER_LINK_NAME = "Link: ";
    //const char *HEADER_CONTENT_TPYE = "Content-type: message/http";
    //const char *HEADER_CONTENT_HTTP = "\r\n";

    /* Find Link option in Http header */
    if( 0 == strncmp(header, HEADER_LINK_NAME, strlen(HEADER_LINK_NAME))) {
        /* get next request url */
        if (parse_link_headers(reversehttp, header) != 0) {
            PRINT_ERR("parse_link_headers");
        }
        
        // PRINT_DEBUG("orginal link string is:%s\n",header);
    } else {
         // PRINT_DEBUG("cb:%s",header);
    }

    return (size_t)(size * nmemb);
}

/*********************************************************************************
    when browser send request to server and server send to this client , 
    curl will invoke this function 
*********************************************************************************/
static size_t capture_body( void *ptr, size_t size, size_t nmemb, void *userdata)
{
    char *body = (char *)ptr;
    struct reversehttp_s *reversehttp = (struct reversehttp_s *)userdata;
    // PRINT_DEBUG("###request from browser:\n%s\n",(char *)ptr);
    char *request = strdup(body);

    /* wipe off label from http path 
        example:
            original header in body: GET /NVE10000/ HTTP/1.1
            we want to actually: GET / HTTP/1.1
    */
    int label_len = strlen(reversehttp->label);
    char *origin = strstr(request,"/"); //skip "GET or POST"
    int offset = origin - request;
    memmove(origin,origin+label_len+1,strlen(request)-offset-label_len-1);
    *(request + strlen(request) - label_len - 1) = '\0';

    PRINT_DEBUG("###request after parse:\n%s\n",request);
    /* it's block. ##TODO: change to asyn */
    //http_redirect(reversehttp,request);
	
	reversehttp->repost_request_header = request;
	reversehttp->need_repost_request = 1;
//    if(request)
//        free(request);

    return (size_t)(size * nmemb);
}

/* Auxiliary function that waits on the socket. */
static int wait_on_socket(curl_socket_t sockfd, int for_recv, long timeout_ms)
{
    struct timeval tv;
    fd_set infd, outfd, errfd;
    int res;

    /* 1 min */
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec= (timeout_ms % 1000) * 1000;

    FD_ZERO(&infd);
    FD_ZERO(&outfd);
    FD_ZERO(&errfd);

    FD_SET(sockfd, &errfd); /* always check for error */

    if(for_recv) {
        FD_SET(sockfd, &infd);
    } else {
        FD_SET(sockfd, &outfd);
    }

    /* select() returns the number of signalled sockets or -1 */
    res = select(sockfd + 1, &infd, &outfd, &errfd, &tv);
    return res;
}

int reversehttp_post(struct reversehttp_s *reversehttp,
					CURL *curl,
					char *url,
					void *body, 
					size_t len, 
					struct curl_slist *custom_headers)
{
    CURLcode res;

    if(curl) {
        /* First set the URL that is about to receive our POST. This URL can
        just as well be a https:// URL if that is what should receive the
        data. */
        curl_easy_setopt(curl, CURLOPT_URL, url);

        /* Now specify we want to POST data */
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        /* Need specify post data length, or default is strlen(body) */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,(curl_off_t)len);

			//printf("1231231231231=%p\n",custom_headers);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, custom_headers);
		
        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK) {
            PRINT_ERR("curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
            return -1;
        }
    }
	
    PRINT_DEBUG("-------------post_done----------------\n");

    return 0;
}

int reversehttp_redirect(struct reversehttp_s *reversehttp, 
						CURL *curl2,
						char *url,
						const char *request_header,
						unsigned char **result,
						size_t *result_len)
{
    CURLcode res;
    curl_socket_t sockfd; /* socket */
    long sockextr;
    size_t iolen = 0;
	
    /* init connenting local host handle */
    reversehttp->local_curl_handle = curl_easy_init();
	
    CURL *curl = reversehttp->local_curl_handle;
#ifdef __DEBUG__
    curl_easy_setopt(reversehttp->local_curl_handle, CURLOPT_VERBOSE, 1);
#endif
    if(curl) {
        puts(url);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        /* Do not do the transfer - only connect to host */
        curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
        
        res = curl_easy_perform(curl);

        if(CURLE_OK != res) {
            printf("Error: %s\n", strerror(res));
            return 1;
        }

        /* Extract the socket from the curl handle - we'll need it for waiting.
         * Note that this API takes a pointer to a 'long' while we use
         * curl_socket_t for sockets otherwise.
         */
        res = curl_easy_getinfo(curl, CURLINFO_LASTSOCKET, &sockextr);
        if(CURLE_OK != res) {
            printf("Error: %s\n", curl_easy_strerror(res));
            return -1;
        }

        sockfd = sockextr;

	/* wait for the socket to become ready for sending */
	if(!wait_on_socket(sockfd, 0, 60000L)) {
	    printf("Error: timeout.\n");
	    return -1;
	}	
		
        puts("Sending request.");
	puts(request_header);
	size_t have_send = 0;
	size_t send_len = strlen(request_header);
	do {
	    res = curl_easy_send(curl, request_header+have_send, send_len-have_send, &iolen);
	    if(CURLE_OK != res) 
	    {
		PRINT_ERR("curl_easy_send:%s", curl_easy_strerror(res));
		break;
	    }
	    have_send+=iolen;
	}while(have_send<send_len);
	PRINT_DEBUG("have send:%zu\n",have_send);
		
        puts("Reading response.");
        unsigned int buffer_size = 50 * 1024;
        unsigned int buffer_raise_step = 50 *1024;
        unsigned char *buffer = (unsigned char *)malloc(buffer_size); //50KB
        int copied = 0;

        /* read the response */
        for(;;) {
	    unsigned char buf[1024];
	    wait_on_socket(sockfd, 1, 30000L);
	    res = curl_easy_recv(curl, buf, 1024, &iolen);
            // PRINT_DEBUG("buf=%s\n",buf);
            if(CURLE_OK != res) 
	    {
                // PRINT_ERR("curl_easy_recv:%s", curl_easy_strerror(res));
                break;
            }

            /* expend memory and copy old data */
            if (copied + iolen > buffer_size) {
                unsigned char *tmp = buffer;
                unsigned int new_buffer_size = buffer_size + buffer_raise_step;
                buffer = (unsigned char *)malloc(new_buffer_size);
                memcpy(buffer,tmp,copied);
                buffer_size = new_buffer_size; //update buffer_size
                free(tmp);  
            }

            /* copy to buffer */    
            memcpy(buffer+copied,buf,iolen);
            copied += iolen;
        }
        //PRINT_DEBUG("----------response:%s\n",buffer);
	PRINT_DEBUG("----------------buffer---size=%d-------\n",copied);
	/* save result */
	*result = buffer;
	*result_len = copied;
	curl_easy_cleanup(curl);
    }
	
    return 0;
}

int http_post(struct reversehttp_s *reversehttp,char *url,void *body, int len, struct curl_slist *custom_headers)
{
    CURL *curl;
    CURLcode res;

    /* get a curl handle */
    curl = curl_easy_init();
    if(curl) {
        /* First set the URL that is about to receive our POST. This URL can
        just as well be a https:// URL if that is what should receive the
        data. */
        curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "identity");
        /* Now specify we want to POST data */
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        /* Need specify post data length, or default is strlen(body) */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,(curl_off_t)len);
        /* header options */
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, capture_header);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, reversehttp);
        
        curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, 1);
        curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1);

        if(custom_headers)
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, custom_headers);

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK) {
            PRINT_ERR("curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
            return -1;
        }
    }
    /* always cleanup */
    curl_easy_cleanup(curl);
    PRINT_DEBUG("-------------post_done----------------\n");

    return 0;
}

int http_get(struct reversehttp_s *reversehttp, char *url, struct curl_slist *headers)
{
    CURL *curl;
    CURLcode res;
    int ret = 0;
	
    curl = curl_easy_init();
	/* Set Callback functions */
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, capture_header);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, capture_body);
        
    /* Set userpoint */    
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, reversehttp);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, reversehttp);
	
#ifdef __DEBUG__
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
#endif
    while(curl) {
    
    /* set URL to get */
    curl_easy_setopt(curl, CURLOPT_URL, url);

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    
        /* Check for errors */
        if(res != CURLE_OK) {
    	    PRINT_ERR("curl_easy_perform() failed: %s",
    	    curl_easy_strerror(res));
    	    ret = -1;
    	    break;
	}
    }
    /* always cleanup */
    curl_easy_cleanup(curl);
    PRINT_DEBUG("------------------get_done---------------\n");

    return ret;
}

int reversehttp_get(struct reversehttp_s *reversehttp, 
				    CURL *curl,
				    char *url)
{
    CURLcode res;

    if(curl) {
    	PRINT_DEBUG("get url:%s\n",url);
        /* set URL to get */
        curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_POST, 0L);
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "identity");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);
		
        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK) {
            PRINT_ERR("curl_easy_perform() failed: %s",
            curl_easy_strerror(res));
	    return -1;
        }
    }

    PRINT_DEBUG("------------------get_done---------------\n");

    return 0;
}

/* redirect request from server to localhost and response to server */
int http_redirect(struct reversehttp_s *reversehttp, const char *request)
{
    CURL *curl;
    CURLcode res;

    curl_socket_t sockfd; /* socket */
    long sockextr;
    size_t iolen;
    curl_off_t nread;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, reversehttp->localhost_url);
        /* Do not do the transfer - only connect to host */
        curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        res = curl_easy_perform(curl);

        if(CURLE_OK != res) {
            printf("Error: %s\n", strerror(res));
            return 1;
        }

        /* Extract the socket from the curl handle - we'll need it for waiting.
         * Note that this API takes a pointer to a 'long' while we use
         * curl_socket_t for sockets otherwise.
         */
        res = curl_easy_getinfo(curl, CURLINFO_LASTSOCKET, &sockextr);

        if(CURLE_OK != res) {
            printf("Error: %s\n", curl_easy_strerror(res));
            return 1;
        }

        sockfd = sockextr;

        /* wait for the socket to become ready for sending */
        if(wait_on_socket(sockfd, 0, 60000L) < 0) {
            PRINT_ERR("Error: timeout.");
            return 1;
        }

        puts("Sending request.");
        /* Send the request. Real applications should check the iolen
         * to see if all the request has been sent */
        res = curl_easy_send(curl, request, strlen(request), &iolen);
        if(CURLE_OK != res) {
            PRINT_ERR("curl_easy_send:%s", curl_easy_strerror(res));
            return 1;
        }
        puts("Reading response.");

        unsigned int buffer_size = 50 * 1024;
        unsigned int buffer_raise_step = 50 *1024;
        unsigned char *buffer = (unsigned char *)malloc(buffer_size); //50KB
        int copied = 0;
        /* read the response */
        for(;;) {
            unsigned char buf[1024];
            wait_on_socket(sockfd, 1, 60000L);
            res = curl_easy_recv(curl, buf, 1024, &iolen);
            // PRINT_DEBUG("buf=%s\n",buf);
            if(CURLE_OK != res) {
                PRINT_ERR("curl_easy_recv:%s", curl_easy_strerror(res));
                break;
            }

            /* expend memory and copy old data */
            if (copied + iolen > buffer_size) {
                unsigned char *tmp = buffer;
                unsigned int new_buffer_size = buffer_size + buffer_raise_step;
                buffer = (unsigned char *)malloc(new_buffer_size);
                memcpy(buffer,tmp,copied);
                buffer_size = new_buffer_size; //update buffer_size
                free(tmp);  
            }

            /* copy to buffer */    
            memcpy(buffer+copied,buf,iolen);
            copied += iolen;

            nread = (curl_off_t)iolen;
            printf("Received %" CURL_FORMAT_CURL_OFF_T " bytes.\n", nread);
        }
        //PRINT_DEBUG("----------response:%s\n",buf);
        
        printf("----------------buffer---size=%d-------\n",copied);

        struct curl_slist *chunk = NULL;
        curl_slist_append(chunk, "Content-type: message/http");

        http_post(reversehttp,reversehttp->next_request_url, buffer,copied,chunk);

        /* free the custom headers */
        curl_slist_free_all(chunk);

        /* free buffer */
        if (buffer)
            free(buffer);

        /* always cleanup */
        curl_easy_cleanup(curl);
    }
    return 0;
}

int http_get_loop(struct reversehttp_s *reversehttp)
{
    CURL *curl;
    CURLcode res;

    curl_socket_t sockfd; /* socket */
    long sockextr;
    size_t iolen;
//    curl_off_t nread;

    curl = curl_easy_init();
    if(curl) {
	puts(reversehttp->next_request_url);
        curl_easy_setopt(curl, CURLOPT_URL, reversehttp->next_request_url);
        /* Do not do the transfer - only connect to host */
        curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        res = curl_easy_perform(curl);

        if(CURLE_OK != res) {
            printf("Error: %s\n", strerror(res));
            return 1;
        }

        /* Extract the socket from the curl handle - we'll need it for waiting.
         * Note that this API takes a pointer to a 'long' while we use
         * curl_socket_t for sockets otherwise.
         */
        res = curl_easy_getinfo(curl, CURLINFO_LASTSOCKET, &sockextr);

        if(CURLE_OK != res) {
            printf("Error: %s\n", curl_easy_strerror(res));
            return 1;
        }

        sockfd = sockextr;
	if(wait_on_socket(sockfd, 0, 60000L) < 0) {
	    PRINT_ERR("Error: timeout.");
	    return 1;
	}
	for(;;)
	{
	    /* wait for the socket to become ready for sending */
	    char *path = strstr(reversehttp->next_request_url, "/admin/");
	    char request[1024] = {0};
	    sprintf(request,"GET %s HTTP/1.1\r\nHost: 192.168.2.109:8000\r\nAccept-Encoding: identity\r\n\r\n",path);
	    puts("Sending request.");
	    puts(request);
	    
	    /* Send the request. Real applications should check the iolen
	     * to see if all the request has been sent */
	     printf("strlen(request)=%d\n",strlen(request));
	     res = curl_easy_send(curl, request, strlen(request), &iolen);
	     if(CURLE_OK != res) {
	      	PRINT_ERR("curl_easy_send:%s", curl_easy_strerror(res));
		return 1;
	     }
	     puts("Reading response.");
	     
	     unsigned int buffer_size = 50 * 1024;
	     unsigned int buffer_raise_step = 50 *1024;
	     unsigned char *buffer = (unsigned char *)malloc(buffer_size); //50KB
	     int copied = 0;
	     wait_on_socket(sockfd, 1, 30000L);
	     
	     /* read the response */
	     for(;;) {
	     	unsigned char buf[1024];
	     	
	     	res = curl_easy_recv(curl, buf, 1024, &iolen);
	     	// PRINT_DEBUG("buf=%s\n",buf);
	     	if(CURLE_OK != res) {
	     	    //PRINT_ERR("curl_easy_recv:%s", curl_easy_strerror(res));
	     	    break;
	     	}

                /* expend memory and copy old data */
                if (copied + iolen > buffer_size) {
		    unsigned char *tmp = buffer;
		    unsigned int new_buffer_size = buffer_size + buffer_raise_step;
		    buffer = (unsigned char *)malloc(new_buffer_size);
		    memcpy(buffer,tmp,copied);
		    buffer_size = new_buffer_size; //update buffer_size
		    free(tmp);  
                }

		/* copy to buffer */    
		memcpy(buffer+copied,buf,iolen);
		copied += iolen;
		printf("----------------buffer---size=%d-------\n",copied);
	     }
	     
	     /* free buffer */
	     if (buffer)
	         free(buffer);
        }
        
#if 0
        struct curl_slist *chunk = NULL;
        curl_slist_append(chunk, "Content-type: message/http");

        http_post(reversehttp,reversehttp->next_request_url, buffer,copied,chunk);

        /* free the custom headers */
        curl_slist_free_all(chunk);
#endif

        /* always cleanup */
        curl_easy_cleanup(curl);
    }
    return 0;
}

int routine(struct reversehttp_s *reversehttp )
{
    assert(reversehttp);
    
    int ret = 0;
    char post_fields[64];
    int post_fields_len;
	
    post_fields_len = snprintf(post_fields,sizeof(post_fields),"name=%s&token=-", 
    reversehttp->label);
    /* main loop */
    for(;;)
    {
    	/* register vhost by using POST method */
    	if ( 0 != reversehttp_post(reversehttp,
    	                           reversehttp->remote_curl_handle,
    	                           reversehttp->endpoint,
    	                           post_fields,
    	                           post_fields_len,
    	                           NULL) ) {
    	    PRINT_ERR("reversehttp POST error,try again in 30secs later");
    	    sleep(30);
    	    continue;
    	}
    	
    	/* GET request forever */
    	for(;;) {
    	    if ( 0 != reversehttp_get(reversehttp,
    				      reversehttp->remote_curl_handle,
    				      reversehttp->next_request_url) ) {
    	        PRINT_ERR("reversehttp GET, reset in 10secs later");
    	        sleep(10);
    	        break;
    				      	
    	    } else {
    		unsigned char *response = NULL;
    		size_t resp_len = 0;
    		if(reversehttp->need_repost_request) {
    		    reversehttp->need_repost_request = 0;
    		
    		    /* get local host response */
    		    if (0 != reversehttp_redirect(reversehttp,
    						  reversehttp->local_curl_handle,
    						  reversehttp->localhost_url,
    						  reversehttp->repost_request_header,
    						  &response,
    						  &resp_len) ) {
    		        PRINT_ERR("reversehttp redirect error");
    			/* free buffer */
    			if (response)
    			    free(response);
    			    
    			continue;
    		    }
    				
    	            if (resp_len>0) {
    		        /* post response to server */
    		        struct curl_slist *chunk = NULL;
    		        chunk = curl_slist_append(chunk, "Content-type: message/http");
    		        chunk = curl_slist_append(chunk, "Expect:");
    		        reversehttp_post(reversehttp,
    		        		     reversehttp->remote_curl_handle,
    		        		     reversehttp->next_request_url,
    		        		     response,
    		        		     resp_len,
    		        		     chunk);    
    		        /* free the custom headers */
    		        curl_slist_free_all(chunk);
    		        
    		        /* update next request url */
    		        if (reversehttp->next_request_url) 
    		        	free(reversehttp->next_request_url);	
    		        	
    		        reversehttp->next_request_url = strdup(reversehttp->next_request_url_tmp);
    		    }
    		    		
    		    /* free buffers */
    		    if (response)
    		        free(response);
    		    if (reversehttp->repost_request_header) {
    		        free(reversehttp->repost_request_header);
    		        reversehttp->repost_request_header = NULL;
    		    }						    
    		} /*if need_repost_request*/
            } /* if get request succeed */
    	}/* get loop */
    } /* main loop */
    
    return ret;
}

int init_curl_handles(struct reversehttp_s *reversehttp)
{
    /* curl init CURL_GLOBAL_ALL */
    curl_global_init(CURL_GLOBAL_ALL); 
 
    int ret = 0;
	
    /* init connenting remote server handle */
    reversehttp->remote_curl_handle = curl_easy_init();
    /* Set Callback functions */
    curl_easy_setopt(reversehttp->remote_curl_handle, CURLOPT_HEADERFUNCTION, capture_header);
    curl_easy_setopt(reversehttp->remote_curl_handle, CURLOPT_WRITEFUNCTION, capture_body);
          
    /* Set userpoint */    
    curl_easy_setopt(reversehttp->remote_curl_handle, CURLOPT_HEADERDATA, reversehttp);
    curl_easy_setopt(reversehttp->remote_curl_handle, CURLOPT_WRITEDATA, reversehttp);
	
#ifdef __DEBUG__
    curl_easy_setopt(reversehttp->remote_curl_handle, CURLOPT_VERBOSE, 1L);
#endif

    return ret;
}

