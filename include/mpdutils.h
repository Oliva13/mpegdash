#ifndef __MPD_UTILS_H__
#define __MPD_UTILS_H__
typedef struct
{
	char *path_file;
	char *path_segfile;
	int64_t timestamp;
	struct list_head link;
	pthread_mutex_t mpd_mtx;
	
}mpd_t;

#define id_cliend_last(head) list_entry(list_last(head), mpd_t, link)

//struct thread_args
//{
//    pthread_cond_t	  thread_flag_cv;
//    pthread_mutex_t	  thread_flag_mutex;
//    void		  	 *data_pthread;
//    int			      thread_flag;
//    mpd_t      	  *mpd_file;
//};
extern struct list_head gl_mpd_link_stream0, gl_mpd_link_stream1;
extern pthread_mutex_t mtx;
extern pthread_cond_t cond;
extern pthread_mutex_t stm0_mpd_mtx;
extern pthread_mutex_t stm1_mpd_mtx;

#ifdef __cplusplus
extern "C" {
#endif

//extern struct list_head gl_mpd_link;
//void del_mpd(mpd_t *mpd_file);
unsigned long get_uptime(void);
unsigned long long monotonic_ms(void);
//void init_start(struct thread_args *ptr);
//void init_close(struct thread_args *ptr);
//void *create_mpd(void *arg);
//void *thr_del_mpd(void *arg);
#ifdef __cplusplus
}
#endif
#endif










