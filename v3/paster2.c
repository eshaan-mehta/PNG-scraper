#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/shm.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include "./png_util3/lab_png.h"
#include "./png_util3/zutil.h"  
#include <time.h>

#define ECE252_HEADER "X-Ece252-Fragment: "
#define STRIP_SIZE 10000
#define STRIP_INC 5000
#define INF_IDAT_SIZE 6*(400*4 + 1)

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef struct image_strip2 {
    U8 buf[10000];       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} IMAGE_STRIP;


typedef struct strip_stack
{
    int size;               /* the max capacity of the stack */
    int pos;                /* position of last item pushed onto the stack */
    IMAGE_STRIP *items;             /* stack of stored integers */
} RING_BUFFER;

int sizeof_shm_stack(int size) {
    return (sizeof(RING_BUFFER) + sizeof(IMAGE_STRIP) * size);
}

/**
 * @brief initialize the RING_BUFFER member fields.
 * @param RING_BUFFER *p points to the starting addr. of an RING_BUFFER struct
 * @param int stack_size max. number of items the stack can hold
 * @return 0 on success; non-zero on failure
 * NOTE:
 * The caller first calls sizeof_shm_stack() to allocate enough memory;
 * then calls the init_shm_stack to initialize the struct
 */
int init_shm_stack(RING_BUFFER *p, int stack_size)
{   
    if ( p == NULL || stack_size == 0 ) {
        return 1;
    }

    p->size = stack_size;
    p->pos = -1;
    p->items = (IMAGE_STRIP *) ((char *)p + sizeof(RING_BUFFER));
    return 0;
}

void destroy_stack(RING_BUFFER *p)
{
    if ( p != NULL ) {
        free(p);
    }
}

int is_full(RING_BUFFER *p)
{
    if ( p == NULL ) {
        return 0;
    }
    return ( p->pos == (p->size - 1) );
}

int is_empty(RING_BUFFER *p)
{
    if ( p == NULL ) {
        return 0;
    }
    return ( p->pos == -1 );
}

int push(RING_BUFFER *p, IMAGE_STRIP item)
{
    if ( p == NULL ) {
        return -1;
    }

    if ( !is_full(p) ) {
        ++(p->pos);
        p->items[p->pos] = item;
        return 0;
    } else {
        return -1;
    }
}

int pop(RING_BUFFER *p, IMAGE_STRIP *p_item)
{
    if ( p == NULL ) {
        return -1;
    }

    if ( !is_empty(p) ) {
        *p_item = p->items[p->pos];
        (p->pos)--;
        return 0;
    } else {
        return 1;
    }
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
    IMAGE_STRIP *p = (IMAGE_STRIP *)p_userdata;
 
    // if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */ 
    //     /* received data is not 0 terminated, add one byte for terminating 0 */
    //     size_t new_size = p->max_size + max(STRIP_INC, realsize + 1);   
    //     char *q = realloc(p->buf, new_size);
    //     if (q == NULL) {
    //         perror("realloc"); /* out of memory */
    //         return -1;
    //     }
    //     memcpy(p->buf, q, p->size); /*copy old data to new buffer*/
    //     p->max_size = new_size;
    // }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    IMAGE_STRIP *p = userdata;
    
    if (realsize > strlen(ECE252_HEADER) &&
	strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0) {

        /* extract img sequence number */
	p->seq = atoi(p_recv + strlen(ECE252_HEADER));

    }
    return realsize;
}

int image_strip_init(IMAGE_STRIP *ptr, size_t max_size)
{
    
    if (ptr == NULL) {
        return 1;
    }
    
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1;              /* valid seq should be non-negative */
    return 0;
}

int image_strip_cleanup(IMAGE_STRIP *ptr)
{
    if (ptr == NULL) {
	    return 1;
    }
    
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

void producer_work(int b, int n, int p, int c, RING_BUFFER* shm_ring_buffer, int* next_strip_to_fetch, sem_t* buf_spaces_avail, sem_t* buf_spaces_occ, pthread_mutex_t* ring_buffer_mutex) {
    while (1){
        sem_wait(buf_spaces_avail);
        
        int strip_to_get = __sync_fetch_and_add(next_strip_to_fetch, 1); // atomic increment
        if (strip_to_get >= 50) {
            sem_post(buf_spaces_avail); // signal semaphore for remaining producers
            break;
        }

        // create URL for request
        char url[256];
        sprintf(url, "http://ece252-1.uwaterloo.ca:2530/image?img=%d&part=%d", n, strip_to_get);

        IMAGE_STRIP strip;
        image_strip_init(&strip, STRIP_SIZE); // max size for a strip is 10KB

        // setup curl handle
        CURL *curl_handle = curl_easy_init();
        if (curl_handle == NULL) {
            __sync_fetch_and_sub(next_strip_to_fetch, 1); // decrement strip count
            continue;
        }

        // setup request
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&strip);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&strip);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        // perform request
        CURLcode res = curl_easy_perform(curl_handle);
        if (res != CURLE_OK) {
            __sync_fetch_and_sub(next_strip_to_fetch, 1); // decrement strip count
            curl_easy_cleanup(curl_handle);
            continue;
        }

        // save strip to ring buffer
        pthread_mutex_lock(ring_buffer_mutex); 
        int ret = push(shm_ring_buffer, strip);
        pthread_mutex_unlock(ring_buffer_mutex);

        // push error handling        
        if (ret != 0) {
            __sync_fetch_and_sub(next_strip_to_fetch, 1); // decrement strip count
            sem_post(buf_spaces_avail);
            continue;
        }

        // post for consumer to process
        sem_post(buf_spaces_occ);
        curl_easy_cleanup(curl_handle);
    }

    _exit(0);
}

void consumer_work(int b, int x, int p, int c, RING_BUFFER* shm_ring_buffer, U8* final_strips, int* strips_processed_count, sem_t* buf_spaces_avail, sem_t* buf_spaces_occ, pthread_mutex_t* ring_buffer_mutex) {
    while (1) {
        sem_wait(buf_spaces_occ);  

        IMAGE_STRIP strip;
        image_strip_init(&strip, STRIP_SIZE);
        
        // pop strip from ring buffer
        pthread_mutex_lock(ring_buffer_mutex);
        if (is_empty(shm_ring_buffer)) {
            pthread_mutex_unlock(ring_buffer_mutex);
            
            sem_post(buf_spaces_occ); // release semaphore for next consumer

            // all strips processed
            if (*strips_processed_count >= 50) { 
                pthread_mutex_unlock(ring_buffer_mutex);
                break;
            }
            else { continue; } // no strip to process atm
        }
        int ret = pop(shm_ring_buffer, &strip); // get strip from buffer
        pthread_mutex_unlock(ring_buffer_mutex);

        // pop error handling
        if (ret != 0) {
            sem_post(buf_spaces_occ);
            continue;
        }
        sem_post(buf_spaces_avail);

        struct timespec ts;
        ts.tv_sec = x / 1000; // convert milliseconds to seconds
        ts.tv_nsec = (x % 1000) * 1000000; // convert remainder to nanoseconds
        nanosleep(&ts, NULL);

        simple_PNG_p curr = mallocPNG();
        if (get_png_chunks(curr, strip.buf, 8) != 0) {
            free_png(curr);
            continue;
        };

        U64 inflated_size;
        U8 inflated_buf[INF_IDAT_SIZE];
        mem_inf(inflated_buf, &inflated_size, curr->p_IDAT->p_data, curr->p_IDAT->length);
        memcpy(&final_strips[strip.seq*INF_IDAT_SIZE], inflated_buf, inflated_size); // save strip to shared memory

        __sync_fetch_and_add(strips_processed_count, 1);  // increment strips processed count
        if (*strips_processed_count >= 50) {
            // Signal other consumers to exit
            for (int i = 0; i < c; i++) {
                sem_post(buf_spaces_occ);
            }
        }

        free_png(curr);
    }
    _exit(0);
}

int main(int argc, char** argv) {
    double times[2];
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;
    
    int b = atoi(argv[1]);
    int p = atoi(argv[2]);
    int c = atoi(argv[3]);
    int x = atoi(argv[4]);
    int n = atoi(argv[5]);

    if (argc < 6) {
        fprintf(stderr, "Usage: %s Not enough arguments.\n", argv[0]);
        return 1;
    }

    if (b < 1 || b > 50 || p < 1 || p > 20 || c < 1 || c > 20 || x < 0 || x > 1000 || n < 1 || n > 3) {
        fprintf(stderr, "Usage: %s Invalid arguments.\n", argv[0]);
        return 1;
    }

    pid_t cpid;
    pid_t ppid=0;
    pid_t ppids[p];
    pid_t cpids[c];

    curl_global_init(CURL_GLOBAL_DEFAULT);

    // initialize shared memory for ring_buffer mutex
    int shmid_ring_buffer_mutex = shmget(IPC_PRIVATE, sizeof(pthread_mutex_t), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    pthread_mutex_t* ring_buffer_mutex = shmat(shmid_ring_buffer_mutex, NULL, 0);
    pthread_mutexattr_t mutexAttr;
    pthread_mutexattr_init(&mutexAttr);
    pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(ring_buffer_mutex, &mutexAttr);
    pthread_mutexattr_destroy(&mutexAttr);

    // initialize shared memory for buf_spaces_avail semaphore
    int shmid_buf_spaces_avail = shmget(IPC_PRIVATE, sizeof(sem_t), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    sem_t* buf_spaces_avail = shmat(shmid_buf_spaces_avail, NULL, 0);
    sem_init(buf_spaces_avail, 1, b);

    // initialize shared memory for buf_spaces_occ semaphore
    int shmid_buf_spaces_occ = shmget(IPC_PRIVATE, sizeof(sem_t), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    sem_t* buf_spaces_occ = shmat(shmid_buf_spaces_occ, NULL, 0);
    sem_init(buf_spaces_occ, 1, 0); // init with locked value

    // initialize shared memory for ring buffer
    int shm_size = sizeof_shm_stack(b);
    int shmid_ring_buffer = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR); 
    RING_BUFFER *ring_buffer = shmat(shmid_ring_buffer, NULL, 0);
    init_shm_stack(ring_buffer, b); 
    
    // initialize the array in shared memory to know what strips have been worked on
    int shmid_next_strip = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int *next_strip_to_fetch = shmat(shmid_next_strip, NULL, 0);
    *next_strip_to_fetch = 0; // Start from strip 0

    // Initialize shared memory for the processed strips count
    int shmid_strips_processed = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int *strips_processed_count = shmat(shmid_strips_processed, NULL, 0);
    *strips_processed_count = 0; // No strips processed initially

    // Initialize shared memory for strips worked on
    int shmid_final_strips = shmget(IPC_PRIVATE, 50*INF_IDAT_SIZE, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    U8 *final_strips = shmat(shmid_final_strips, NULL, 0);
    memset(final_strips, 0, 50*INF_IDAT_SIZE);
    
    // create producer processes
    for (int i = 0; i < p; i++) {
        ppid = fork();

        if ( ppid > 0 ) {        /* parent proc */
            ppids[i] = ppid;
        } else if ( ppid == 0 ) { /* child proc */
            producer_work(b, n, p, c, ring_buffer, next_strip_to_fetch, buf_spaces_avail, buf_spaces_occ, ring_buffer_mutex);
            break; // prevent child from forking
        } else {
            perror("forking producer");
            abort();
        }
    }

    // actions for only parent process
    if (ppid > 0) {
        // create consumer processes 
        for (int i = 0; i < c; i++) {
            cpid = fork();

            if ( cpid > 0 ) {        /* parent proc */
                cpids[i] = cpid;
            } else if ( cpid == 0 ) { /* child proc */
                consumer_work(b, x, p, c, ring_buffer, final_strips, strips_processed_count, buf_spaces_avail, buf_spaces_occ, ring_buffer_mutex);
                break; // prevent child from forking
            } else {
                perror("forking consumer");
                abort();
            }
        }

        // wait for all producer processes to finish
        for (int i = 0; i < p; i++) {
            waitpid(ppids[i], NULL, 0);
        }

        // wait for all consumer processes to finish
        for (int i = 0; i < c; i++) {
            waitpid(cpids[i], NULL, 0);
        }
    }
    
    // construct and save final image
    struct simple_PNG ans;
    ans.p_IHDR = malloc(sizeof(struct chunk));
    ans.p_IDAT = malloc(sizeof(struct chunk));
    ans.p_IEND = malloc(sizeof(struct chunk));

    // create new IHDR chunk
    ans.p_IHDR->length = DATA_IHDR_SIZE;
    strncpy(ans.p_IHDR->type, "IHDR", 4);
    ans.p_IHDR->p_data = malloc(DATA_IHDR_SIZE);

    int width = 400;
    int height = 50*6;
    
    // Populate the IHDR chunk
    ans.p_IHDR->length = DATA_IHDR_SIZE;
    *(U32*)(ans.p_IHDR->p_data) = htonl(width); // width
    *(U32*)(ans.p_IHDR->p_data + 4) = htonl(height); // height
    *(ans.p_IHDR->p_data + 8) = 8; // bit depth
    *(ans.p_IHDR->p_data + 9) = 6; // color type
    *(ans.p_IHDR->p_data + 10) = 0; // compression
    *(ans.p_IHDR->p_data + 11) = 0; // filter
    *(ans.p_IHDR->p_data + 12) = 0; // interlace
    ans.p_IHDR->crc = htonl(calculate_chunk_crc(ans.p_IHDR));

    U8* deflated_IDAT = malloc(50*INF_IDAT_SIZE);
    U64 deflated_IDAT_size;

    // create new IDAT chunk
    strncpy(ans.p_IDAT->type, "IDAT", 4);
    mem_def(deflated_IDAT, &deflated_IDAT_size, final_strips, 50*INF_IDAT_SIZE, Z_DEFAULT_COMPRESSION);
    ans.p_IDAT->p_data = deflated_IDAT;
    ans.p_IDAT->length = (U32)deflated_IDAT_size;
    ans.p_IDAT->crc = htonl(calculate_chunk_crc(ans.p_IDAT));

    // create new IEND chunk
    ans.p_IEND->length = 0;
    strncpy(ans.p_IEND->type, "IEND", 4);
    ans.p_IEND->crc = htonl(calculate_chunk_crc(ans.p_IEND));

    // write final image to file
    write_PNG("all.png", &ans);

    // clean up shared memory
    free(ans.p_IHDR->p_data);
    free(ans.p_IHDR);
    free(ans.p_IDAT->p_data);
    free(ans.p_IDAT);
    free(ans.p_IEND);

    pthread_mutex_destroy(ring_buffer_mutex);
    sem_destroy(buf_spaces_avail);
    sem_destroy(buf_spaces_occ);

    shmdt(ring_buffer_mutex);
    shmctl(shmid_ring_buffer_mutex, IPC_RMID, NULL);
    shmdt(ring_buffer);
    shmctl(shmid_ring_buffer, IPC_RMID, NULL);
    shmdt(next_strip_to_fetch);
    shmctl(shmid_next_strip, IPC_RMID, NULL);
    shmdt(strips_processed_count);
    shmctl(shmid_strips_processed, IPC_RMID, NULL);
    shmdt(final_strips);
    shmctl(shmid_final_strips, IPC_RMID, NULL);
    shmdt(buf_spaces_avail);
    shmctl(shmid_buf_spaces_avail, IPC_RMID, NULL);
    shmdt(buf_spaces_occ);
    shmctl(shmid_buf_spaces_occ, IPC_RMID, NULL);
    
    curl_global_cleanup();
    
    struct timeval tv_end;
    if (gettimeofday(&tv_end, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }

    times[1] = (tv_end.tv_sec) + tv_end.tv_usec/1000000.;
    printf("paster2 execution time: %.2lf seconds\n", getpid(),  times[1] - times[0]);

    return 0;
}
