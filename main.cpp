#include "dash-mpd.h"
#include "dash-proto.h"
#include "mov-reader.h"
#include "mov-format.h"
#include "fmp4-writer.h"
#include "mpeg4-avc.h"
#include "path.h"
#include <sys/stat.h>
#include <unordered_map>
#include <sys/time.h>
#include <signal.h>
#include <atomic>
#include <unistd.h>
#include <sys/types.h>
#include "FileWritingLibrary.h"
#include "recorder.h"
#include "mpdutils.h"
#include "ipc.h"

//#define gettid() syscall(SYS_gettid)

volatile sig_atomic_t pthread_start = 0;
volatile sig_atomic_t done = 1, count_pthread = 0;

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t stm0_mpd_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stm1_mpd_mtx = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t  cond = PTHREAD_COND_INITIALIZER;

rec_video_t *param_video_stream_0;
rec_video_t *param_video_stream_1;

#define id_cliend_next(head) list_entry(list_next(head), mpd_t, link)
#define id_cliend_prev(head) list_entry(list_prev(head), mpd_t, link)

struct list_head gl_mpd_link_stream0, gl_mpd_link_stream1;

static void on_sigterm(int pd)
{
	done = 0;
}

int main(int argc, char *argv[])
{
	unsigned int res = 0, status = 0;
	//expired = 0;
	struct timespec cond_time;
	
	LIST_INIT_HEAD(&gl_mpd_link_stream0);
	LIST_INIT_HEAD(&gl_mpd_link_stream1);

#if defined(DEBUG_LAST) 		
	printf("start0: gl_mpd_link - %p\n", &gl_mpd_link_stream0);
	printf("start1: gl_mpd_link - %p\n", &gl_mpd_link_stream1);
#endif	

	static pthread_t mpd_thr_create_stream_0;
	static pthread_t mpd_thr_create_stream_1;
	static pthread_t mpd_thr_del;

	signal(SIGINT, on_sigterm);
	//signal(SIGTERM, on_sigterm);
		
	while(ipc_wait("MON") != 0)
	{
		if(!done)
		{
			printf("exit ipc_wait(MON)\n");
			return status;
		}
	}

	mkdir(PREFIX_MDASH, 0666);

	cfg_subscribe("stream0", on_sigterm, 0);
	cfg_subscribe("stream1", on_sigterm, 1);
	cfg_subscribe("stream2", on_sigterm, 2);
	cfg_subscribe("stream3", on_sigterm, 3);
	
	param_video_stream_0 = (rec_video_t *)malloc(sizeof(rec_video_t));
	if(param_video_stream_0 == 0)
	{
		LOGE("Error malloc param_video_stream_0 - 0x%x\n", status);
		return status;
	}
	
	param_video_stream_1 = (rec_video_t *)malloc(sizeof(rec_video_t));
	if (param_video_stream_1 == 0)
	{
		LOGE("Error malloc param_video_stream_1 - 0x%x\n", status);
		free(param_video_stream_0);
		return status;
	}
	
	memset(param_video_stream_0, 0, sizeof(rec_video_t));
	memset(param_video_stream_1, 0, sizeof(rec_video_t));
	
	status = get_param(VSTREAM0, param_video_stream_0);
	if(status < 0 && status != FW_DISABLED_STREAM)
	{
		LOGE("Error get_param Stream0 - 0x%x\n", status);
		goto error;
	}
	
	status = get_param(VSTREAM1, param_video_stream_1);
	if(status < 0 && status != FW_DISABLED_STREAM)
	{
		LOGE("Error get_param Stream1 - 0x%x\n", status);
		goto error;
	}	
	
	if(param_video_stream_0->stream_active)
	{
		count_pthread = 1;
	}
	
	if(param_video_stream_1->stream_active)
	{
		count_pthread = 2;
	}
	
	//subscribe_change_param_video(VSTREAM0);
	//subscribe_change_param_video(VSTREAM1);

#if defined(DEBUG_LAST) 		
	LOGE("start threads............................\n");
#endif 			

	param_video_stream_0->start_init = false;
	param_video_stream_0->vStream = 0;
	if(param_video_stream_0->stream_active)
	{	
		status = pthread_create(&mpd_thr_create_stream_0, NULL, dash_server_worker, param_video_stream_0);
		if(status)
		{
			LOGE("Error pthread0_create - 0x%x\n", status);
			goto error;
		}
		LOGE("start threads 0............................\n");
	}
	param_video_stream_1->start_init = false;
	param_video_stream_1->vStream = 1;
	
	if(param_video_stream_1->stream_active)
	{	
		status = pthread_create(&mpd_thr_create_stream_1, NULL, dash_server_worker, param_video_stream_1);
		if(status)
		{ 
			LOGE("Error pthread1_create - 0x%x\n", status);
			goto error; 
		}
		LOGE("start threads 1............................\n");
	}

#if defined(DEBUG_INFO_DUMP)
	LOGE("wait.....................................\n");
#endif 	
	cond_time.tv_sec = time(NULL) + 5;
    cond_time.tv_nsec = 0;
		
	//pthread_mutex_lock(&mtx);
	while(count_pthread)	
	{
		status = pthread_cond_timedwait(&cond, &mtx, &cond_time);
		if (status == ETIMEDOUT)
		{
			LOGE("Error: Cond0 timedwait expired!!!\n");
			//expired = 1;
	//		pthread_mutex_unlock(&mtx);
			done = 0;
			goto error1;
		 }
		 if (status != 0)
		 {
		      LOGE("Error: Cond0 timedwait\n");
	//		  pthread_mutex_unlock(&mtx);
			  done = 0;
			  goto error1;
		 }
	}
	//pthread_mutex_unlock(&mtx);
//#if defined(DEBUG_INFO_DUMP)	
	LOGI("mpegdash...........run\n");
//#endif 	
//	thread3 удаляет
	status = pthread_create(&mpd_thr_del, NULL,  thr_del_mpd, NULL);
	if(status)
	{
		LOGE("Error mpd_thr_del - 0x%x\n", status);
		done = 0;
		goto error1;	
	}

error1:
	
	printf("done - %d\n", done);

#if defined(DEBUG_LAST) 	
	LOGE("---------------------------1exit  pthread_join1 ------------------------");
#endif	
	if(param_video_stream_0->stream_active)
	{	
		pthread_join(mpd_thr_create_stream_0, NULL);
#if defined(DEBUG_INFO_DUMP)		
		LOGE("---------------------------2exit  pthread_join2 ------------------------");
#endif 
	}
#if defined(DEBUG_LAST) 	
	LOGE("---------------------------3exit  pthread_join3 ------------------------");
#endif	
	if(param_video_stream_1->stream_active)
	{	
		pthread_join(mpd_thr_create_stream_1, NULL);
#if defined(DEBUG_LAST) 			
		LOGE("---------------------------4exit  pthread_join4 ------------------------");
#endif			
	}
	
	del_all_dir_files();

error:	

	free(param_video_stream_0);
	free(param_video_stream_1);
	
	if(status)
	{

#if defined(DEBUG_LAST) 		
		LOGE("-----------------------5exit error main ------------------------");
#endif 
		return (status);
	}
#if defined(DEBUG_LAST) 			
	LOGE("---------------------------6wait  pthread_join3 ------------------------");
#endif 	
	
	pthread_join(mpd_thr_del, NULL);
		
#if defined(DEBUG_LAST) 			
	LOGE("---------------------------7exit  pthread_join3 ------------------------");
#endif 

//#if defined(DEBUG_LAST) 			
	LOGE("----- exit main -----");
//#endif 	
	return status;
}
