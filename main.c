/**********************************************************
Description: Reversehttp for C client, detail:http://reversehttp.net/
Author: EdisonHsu
Mail: 263113565@qq.com
Last-Modify-Date: 2014-03-21
**********************************************************/
#include <getopt.h>
#include <libgen.h>
#include <string.h>
#include "common.h"
#include "reversehttp.h"


/* ------------------------------static functions------------------------------ */
static int get_opt(struct reversehttp_s * reversehttp, int argc, char **argv);
static void usage(int argc, char **argv);
static void update_location(struct reversehttp_s *reversehttp);

/* ------------------------------implementations ------------------------------ */
static void usage(int argc, char **argv)
{
    printf("usage: %s -l label -e remotehost:port/endpoint -h localhost_url\n",argv[0]);
    printf("usage: %s -label label -endpoint remotehost:port/endpoint -localhost_url localhost_url\n",argv[0]);
    printf("\t example: %s -l NVE1000000 -e http://192.168.0.103:8000/admin -h http://127.0.0.1/\n",argv[0]);
}

static int get_opt(struct reversehttp_s * reversehttp, int argc, char **argv)
{
    int long_index = 0;
    int ch;

    const char* short_options = "l:e:h:"; 
    struct option long_options[] = {  
         { "label", required_argument, 0, 'l' },  
         { "endpoint", required_argument, 0, 'e' },  
         { "localhost_url", required_argument, 0, 'h' },  
         {  0, 0, 0, 0 },  
    };  

    while((ch = getopt_long(argc,argv,short_options,long_options,&long_index))!= -1) {
        switch(ch) {
            case 'l':
                reversehttp->label = strdup(optarg);
                printf("-l : %s\n",optarg);
                break;
            case 'e':
                reversehttp->endpoint = strdup(optarg);
                printf("-e : %s\n",optarg);
                break;
            case 'h':
                reversehttp->localhost_url = strdup(optarg);
                printf("-h : %s\n",optarg);
                break;
            default:
                usage(argc,argv);
                return -1;
        }
    }

    return 0;
}

static void update_location(struct reversehttp_s *reversehttp)
{
    printf("Serving HTTP on %s\n", reversehttp->location);

}

int main(int argc, char **argv)
{
    if (argc < 2) 
	{
        usage(argc,argv);
        return -1;
    }
	
	/* open logs */
    char *basec = strdup(argv[0]);
#ifdef __DEBUG__
    openlog(basename(basec), LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);
#else   
    openlog(basename(basec), LOG_NDELAY | LOG_PID, LOG_USER);
#endif
	
	
	struct reversehttp_s reversehttp = {0};    
    if (get_opt(&reversehttp, argc,argv) != 0)
        return -1;
		
	/* init reversehttp */

	reversehttp.on_location_changed = update_location;
	
	init_curl_handles(&reversehttp);
	
    return routine(&reversehttp);
	
#if 0
    /* destruct */
	curl_easy_cleanup(reversehttp.remote_curl_handle);
	curl_easy_cleanup(reversehttp.local_curl_handle);
    if (reversehttp.endpoint)
        free(reversehttp.endpoint);
    if (reversehttp.label)
        free(reversehttp.label);
    if (reversehttp.localhost_url)
        free(reversehttp.localhost_url);

    curl_global_cleanup();
    return 0;
#endif
}

