
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/types.h>
#include "./png_util2/lab_png.h"
#include <sys/stat.h>

#define IMG_URL "http://ece252-1.uwaterloo.ca:2520/image?img=1"
#define DUM_URL "https://example.com/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef struct recv_buf2 {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

struct thread_args {             
    int image_num;
    int thread_num;
    RECV_BUF** fragments;
};

struct thread_ret {              
    RECV_BUF *recv_buf;
};

pthread_mutex_t fragments_protection;

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);
int catpng(int num_images, RECV_BUF **fragments);

void *do_work(void *arg);
int main(int argc, char **argv);

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;
    
    if (realsize > strlen(ECE252_HEADER) &&
	strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0) {

        /* extract img sequence number */
	p->seq = atoi(p_recv + strlen(ECE252_HEADER));

    }
    return realsize;
}

/**
 * @brief write callback function to save a copy of received data in RAM.
 *        The received libcurl data are pointed by p_recv, 
 *        which is provided by libcurl and is not user allocated memory.
 *        The user allocated memory is at p_userdata. One needs to
 *        cast it to the proper struct to make good use of it.
 *        This function maybe invoked more than once by one invokation of
 *        curl_easy_perform().
 */
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;
 
    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */ 
        /* received data is not 0 terminated, add one byte for terminating 0 */
        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);   
        char *q = realloc(p->buf, new_size);
        if (q == NULL) {
            perror("realloc"); /* out of memory */
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}

int recv_buf_init(RECV_BUF *ptr, size_t max_size)
{
    void *p = NULL;
    
    if (ptr == NULL) {
        return 1;
    }

    p = malloc(max_size);
    if (p == NULL) {
	return 2;
    }
    
    ptr->buf = p;
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1;              /* valid seq should be non-negative */
    return 0;
}

int recv_buf_cleanup(RECV_BUF *ptr)
{
    if (ptr == NULL) {
	    return 1;
    }
    
    free(ptr->buf);
    ptr->size = 0;
    ptr->max_size = 0;
    return 0;
}

/**
 * @brief output data in memory to a file
 * @param path const char *, output file path
 * @param in  void *, input data to be written to the file
 * @param len size_t, length of the input data in bytes
 */

int write_file(const char *path, const void *input, size_t len)
{
    FILE *fp = NULL;

    if (path == NULL) {
        fprintf(stderr, "write_file: file name is null!\n");
        return -1;
    }

    if (input == NULL) {
        fprintf(stderr, "write_file: input data is null!\n");
        return -1;
    }

    fp = fopen(path, "wb");
    if (fp == NULL) {
        perror("fopen");
        return -2;
    }

    if (fwrite(input, 1, len, fp) != len) {
        fprintf(stderr, "write_file: imcomplete write!\n");
        return -3; 
    }
    return fclose(fp);
}

// helper funciton to see if all fragments have been received
int is_full(RECV_BUF **fragments) {
    pthread_mutex_lock(&fragments_protection);

    for (int i=0; i<50; i++) {
        if (fragments[i] == NULL) {
            pthread_mutex_unlock(&fragments_protection);
            return -1;
        }
    }
    
    pthread_mutex_unlock(&fragments_protection);

    return 0;
}

void* do_work(void* arg) {
    struct thread_args *p_in = arg;
    struct thread_ret *p_out = malloc(sizeof(struct thread_ret));

    // create URL for request
    char url[256];
    sprintf(url, "http://ece252-1.uwaterloo.ca:2520/image?img=%d", p_in->image_num);

    while (is_full(p_in->fragments) == -1) {
        CURL *curl_handle;
        CURLcode res;
        RECV_BUF recv_buf;
        recv_buf_init(&recv_buf, BUF_SIZE);

        // setup curl handle
        curl_handle = curl_easy_init();
        if (curl_handle == NULL) {
            fprintf(stderr, "curl_easy_init: returned NULL\n");
            return NULL;
        }

        // setup request
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        // perform request
        res = curl_easy_perform(curl_handle);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s in thread %d\n", curl_easy_strerror(res), p_in->thread_num);
        } else {
            printf("Thread %d: %lu bytes received, seq=%d.\n", p_in->thread_num, recv_buf.size, recv_buf.seq);
        }

        // lock frgaments array before accessing it to prevent race conditions
        pthread_mutex_lock(&fragments_protection);

        // save received data fragment if new
        if (p_in->fragments[recv_buf.seq] == NULL) {
            p_in->fragments[recv_buf.seq] = malloc(sizeof(RECV_BUF));
            p_in->fragments[recv_buf.seq]->buf = malloc(recv_buf.size);
            p_in->fragments[recv_buf.seq]->size = recv_buf.size;
            p_in->fragments[recv_buf.seq]->max_size = recv_buf.max_size;
            p_in->fragments[recv_buf.seq]->seq = recv_buf.seq;
            memcpy(p_in->fragments[recv_buf.seq]->buf, recv_buf.buf, recv_buf.size);
        }
        
        // unlock mutex after fragments array accessing is over
        pthread_mutex_unlock(&fragments_protection);

        //  only populate return on last loop iteration
        if (is_full(p_in->fragments) == 0) {        
            p_out->recv_buf = malloc(sizeof(RECV_BUF));
            p_out->recv_buf->buf = malloc(recv_buf.size);
            p_out->recv_buf->size = recv_buf.size;
            p_out->recv_buf->max_size = recv_buf.max_size;
            p_out->recv_buf->seq = recv_buf.seq;
            memcpy(p_out->recv_buf->buf, recv_buf.buf, recv_buf.size);
        }
    
        // cleanup
        curl_easy_cleanup(curl_handle);
        recv_buf_cleanup(&recv_buf);
    }

    return ((void *)p_out);
}

int main(int argc, char **argv) {
    pthread_mutex_init(&fragments_protection, NULL);
    int t=1, n=1;
    int c;
    char *str = "option requires an argument";
    RECV_BUF* fragments[50];
    memset(fragments, 0, sizeof(fragments));
    printf("argc: %d\n", argc);
    // extract and validate command line args
    while ((c = getopt(argc, argv, "t:n:")) != -1) {
        switch (c) {
        case 't':
	    t = strtoul(optarg, NULL, 10);
	    // printf("option -t specifies a value of %d.\n", t);
	    if (t <= 0 || t > 1000) {
                fprintf(stderr, "%s: %s between 0 and 10 -- 't'\n", argv[0], str);
                return -1;
            }
            break;
        case 'n':
            n = strtoul(optarg, NULL, 10);
	    // printf("option -n specifies a value of %d.\n", n);
            if (n <= 0 || n > 3) {
                fprintf(stderr, "%s: %s 1, 2, or 3 -- 'n'\n", argv[0], str);
                return -1;
            }
            break;
        default:
            return -1;
        }
    }
    printf("t: %d, n: %d\n", t, n);
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // setup thread manager
    pthread_t* p_tids = malloc(sizeof(pthread_t) * t);
    struct thread_args in_params[t];
    struct thread_ret* p_results[t];

    // create all threads
    for (int i=0; i<t; i++) {
        in_params[i].image_num = n;
        in_params[i].thread_num = i;
        in_params[i].fragments = fragments;
        pthread_create(p_tids + i, NULL, do_work, in_params + i);
    }

    // join all threads
    for (int i=0; i<t; i++) {
        pthread_join(p_tids[i], (void **)&(p_results[i]));
        // printf("Thread %d: %lu bytes received, seq=%d.\n", i, p_results[i]->recv_buf->size, p_results[i]->recv_buf->seq);
    }

    int received_all = is_full(fragments);
    if (received_all == 0) {
        printf("All fragments received.\n");

        // catpng all the fragments
        int result = catpng(50, fragments);
        if (result != 0) {
            perror("catpng failed\n");
        } else {
            printf("catpng succeeded\n");
        }
    } else {
        printf("Not all fragments received.\n");
    }

    // cleanup
    curl_global_cleanup();
    free(p_tids);
    for (int i = 0; i < t; i++) {
        if (p_results[i] != NULL) {
            recv_buf_cleanup(p_results[i]->recv_buf);
            free(p_results[i]->recv_buf);
            free(p_results[i]);
        }
    }
    for (int i = 0; i < 50; i++) {
        if (fragments[i] != NULL) {
            recv_buf_cleanup(fragments[i]);
            free(fragments[i]);
        }
    }
    
    pthread_mutex_destroy(&fragments_protection);

    return received_all;
}