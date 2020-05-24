#include <dirent.h>
#include "FileWritingLibrary.h"
#include "dash-mpd.h"
#include "dash-proto.h"
#include "mov-reader.h"
#include "mov-format.h"
#include "fmp4-writer.h"
#include "mpeg4-avc.h"
#include "recorder.h"
#include "mpdutils.h"

#define TIME_OUT_DELETE 5
#define FILES_MPD_COUNT 4

void freeSubsessionBuffer(avi_frame_t *pSubBuffer)
{
	if (pSubBuffer) 
	{
		if (pSubBuffer->data) 
		{
			//LOGE("7878");
			free(pSubBuffer->data);
			//LOGE("7879");
		}
		memset(pSubBuffer, 0, sizeof(avi_frame_t));
	}
}

void del_mpd_stream0(void)
{
	mpd_t *find_mpd = NULL;
	struct list_head *ptr;
	int64_t curr_timestamp = 0, diff_time = 0;		
	
	unsigned int count_mpd = 0; 
	
	//curr_timestamp = get_uptime(); 
	//pthread_mutex_lock(&mtx);
	//pthread_mutex_lock(&del_mpd_mtx);
	pthread_mutex_lock(&stm0_mpd_mtx);	
	if(!list_empty(&gl_mpd_link_stream0))
	{
		list_for_each(ptr, &gl_mpd_link_stream0)
		{
			find_mpd = list_entry(ptr, mpd_t, link);
			//diff_time = (curr_timestamp - find_mpd->timestamp);
			if(count_mpd > FILES_MPD_COUNT && find_mpd != NULL)
			{
#if defined(DEBUG_LAST) 				
				//printf("1. delete: %p, mpd - %s, timestamp - %lld, path_segfile - %s\n", &find_mpd->link, find_mpd->path_file, find_mpd->timestamp, find_mpd->path_segfile);
				printf("1. Stream 0 delete: %p, mpd - %s, path_segfile - %s\n", &find_mpd->link, find_mpd->path_file, find_mpd->path_segfile);
#endif				
				if(find_mpd->path_file != NULL)
				{
					unlink(find_mpd->path_file);
					free(find_mpd->path_file);
				}
				if(find_mpd->path_segfile != NULL)
				{
					unlink(find_mpd->path_segfile);
					free(find_mpd->path_segfile);
				}
				list_remove(&find_mpd->link);
				free(find_mpd);
				break;
			}
			count_mpd++; 
		}
	}
	
	//pthread_mutex_unlock(&del_mpd_mtx);
	pthread_mutex_unlock(&stm0_mpd_mtx);	
	usleep(25000);
}

void del_mpd_stream1(void)
{
	mpd_t *find_mpd = NULL;
	struct list_head *ptr;
	int64_t curr_timestamp = 0, diff_time = 0;		
	
	unsigned int count_mpd = 0; 
		
	//curr_timestamp = get_uptime(); 
	
	pthread_mutex_lock(&stm1_mpd_mtx);		
	if(!list_empty(&gl_mpd_link_stream1))
	{
		list_for_each(ptr, &gl_mpd_link_stream1)
		{
			find_mpd = list_entry(ptr, mpd_t, link);
			//diff_time = (curr_timestamp - find_mpd->timestamp);
			if(count_mpd > FILES_MPD_COUNT && find_mpd != NULL)
			{
#if defined(DEBUG_LAST) 				
				//printf("1. delete: %p, mpd - %s, timestamp - %lld, path_segfile - %s\n", &find_mpd->link, find_mpd->path_file, find_mpd->timestamp, find_mpd->path_segfile);
				printf("1. Stream 1 delete: %p, mpd - %s, path_segfile - %s\n", &find_mpd->link, find_mpd->path_file, find_mpd->path_segfile);
#endif				
				if(find_mpd->path_file != NULL)
				{
					unlink(find_mpd->path_file);
					free(find_mpd->path_file);
				}
				if(find_mpd->path_segfile != NULL)
				{
					unlink(find_mpd->path_segfile);
					free(find_mpd->path_segfile);
				}
				list_remove(&find_mpd->link);
				free(find_mpd);
				break;
			}
			count_mpd++; 
		}
	}
	pthread_mutex_unlock(&stm1_mpd_mtx);		
	usleep(25000);
}


void *thr_del_mpd(void *arg)
{
	int res = 0;
	struct thread_args *p = (struct thread_args *)arg;
	while(done)
	{
		del_mpd_stream0();
		del_mpd_stream1();
	}
	pthread_exit(NULL);
}

int filterf(const struct dirent *entry)
{
    const char *name = entry->d_name;
    if(strcmp(".", name) == 0 || strcmp("..", name) == 0)
    {
		return 0;
    }
    return 1;
}

int del_all_dir_files()
{
    struct dirent **namelist;

    int i,n;

    char del_path[256];

    n = scandir(PREFIX_MDASH, &namelist, &filterf, alphasort);
    if (n < 0)
    {
        LOGE("Error scandir : del_all_dir_files()");
    }
    else
    {
        for (i = 0; i < n; i++)
		{
			snprintf(del_path, sizeof(del_path), "%s%s", PREFIX_MDASH, namelist[i]->d_name);
#if defined(DEBUG_INFO_DUMP)
            printf("------------------ =================> %s\n", del_path);
#endif			
			unlink(del_path);
            free(namelist[i]);
        }
		free(namelist);
    }
    return 0;
}


void *dash_server_worker(void* param)
{
	uint64_t clock = 0;
	int res = 0, count = 0;
	rec_video_t *dash_param = (rec_video_t*)param;
	
	unsigned long long start, now;
	int allsize = 0;
	int unit_size = 0;
	char start_code[4] = {0x0, 0x0, 0x0, 0x1};

	unsigned long track_id = 1;
	uint8_t *offset; 
	int vNoData = 0;
	
	int wait_for_frame = 0;
	
	int FrameNumber = 0;
	//static int num_frame = 1;
	avi_frame_t *vFrame = NULL;
	//avi_frame_t *aFrame = NULL;
	
	vFrame = (avi_frame_t *)malloc(sizeof(avi_frame_t));
	//aFrame = (avi_frame_t *)malloc(sizeof(avi_frame_t));
	
	memset(vFrame, 0, sizeof(avi_frame_t));
	//memset(aFrame, 0, sizeof(avi_frame_t));
#if defined(DEBUG_LAST) 				
	LOGE("Start Stream - %d...............................................................\n", dash_param->vStream);
#endif	
	//FILE *out = fopen("out.h264", "w+t");
    //if (out == NULL)
    //    return 0;
#if defined(DEBUG_LAST) 	
	LOGE("worker: dash_param_video.framerate - %f", dash_param->framerate);
	LOGE("worker: dash_param_video.width - %d", dash_param->width);
	LOGE("worker: dash_param_video.height - %d", dash_param->height);
	LOGE("worker: dash_param_video.vfNumber - %d", dash_param->vfNumber);
	LOGE("worker: dash_param_video.vStream - %d", dash_param->vStream);
	LOGE("worker: init_param_video.encoder - %d", dash_param->encoder);
#endif
	
	FrameNumber = dash_param->vfNumber;
	
	dash_playlist_t *dash;
	dash = new dash_playlist_t();
	count = 0;
	dash->p_rec_video = dash_param; //!!!
#if defined(DEBUG_INFO_DUMP)	
	LOGE("stream_active - %d, vStream - %d\n", dash_param->stream_active, dash_param->vStream);
#endif		
	//LOGE("1");	
	while(done)
	{
	   //if(dash_param->stream_active)
	   //{ 
			res = avi_get_frame(dash_param->vStream, FrameNumber, vFrame);
			//LOGE("dash_server_worker: vFrame = %p, vfNumber = %d, Stream - %d", vFrame, FrameNumber, dash_param->vStream);
			if(res < 0 && res != -ENODATA)
			{			
				LOGE("worker: 1.Failed to get video frame (%p)", res);
				usleep(5000);
				continue;
			}
			else
			{
				if(res != -ENODATA)
				{
					//LOGE("5");
					start = monotonic_ms();
					unsigned char *ptr = vFrame->data;
					int64_t f_pts = vFrame->pts;
					unsigned char *data = ptr;
					allsize = vFrame->size;
					int bytes = allsize;
					wait_for_frame = 0;
				
					while(allsize)
					{
						memcpy(&unit_size, data, 4);
						memcpy(data, start_code, 4);
						//fwrite(start_code, 4, 1, out);
						data += 4;					
						//fwrite(data, 1, unit_size, out);
						data += unit_size;
						allsize -= (unit_size + 4);
					}
				
					if(dash_param->start_init == false)
					{
						if(dash_param->vStream)
						{
#if defined(DEBUG_INFO_DUMP)
						LOGE("init_dash start_1...............................\n");
#endif																				
							init_dash("./start_1", dash, dash_param->vStream);
						}
						else
						{

#if defined(DEBUG_INFO_DUMP)							
							LOGE("init_dash start_0.................................\n");
#endif													
							init_dash("./start_0", dash, dash_param->vStream);
						}
											
#if defined(DEBUG_LAST)							
						LOGE("worker1: init new dash_param_video.framerate - %f", dash_param->framerate);
						LOGE("worker1: init new dash_param_video.width - %d", dash_param->width);
						LOGE("worker1: init new dash_param_video.height - %d", dash_param->height);
						LOGE("worker1: init new dash_param_video.vfNumber - %d", dash_param->vfNumber);
						LOGE("worker1: init new dash_param_video.vStream - %d", dash_param->vStream);
						LOGE("worker2: init_param_video.encoder - %d", dash_param->encoder);
#endif						
						if(dash_param->encoder == 1) 
						{
							init_h264_read_frame(dash, track_id, dash_param->framerate, dash_param->width, dash_param->height, ptr, ptr + bytes);
#if defined(DEBUG_INFO_DUMP)							
							LOGE("init_dash start%d .................................\n", dash_param->vStream);
#endif																				
						}
						else
						{
							if(dash_param->encoder == 0)
							{
								init_h265_read_frame(dash, track_id, dash_param->framerate, dash_param->width, dash_param->height, ptr, ptr + bytes);
#if defined(DEBUG_INFO_DUMP)															
								LOGE("init_dash start%d .................................\n", dash_param->vStream);
#endif																					
							}	
							else
							{
								LOGE("1.Error encoder - %d", dash_param->encoder);
							}
						}
						dash_param->start_init = true;
					}
					//printf("vfNumber = %d, pts - %lld\n", FrameNumber, f_pts);
					//printf("******************* dash_param->encoder - %d *************************\n", dash_param->encoder);
					if(dash_param->encoder == 1) 
					{
						h264_read_frame(track_id, ptr, ptr + bytes, dash, f_pts);
						//LOGE("dash start%d .................................\n", dash_param->vStream);
					}
					else
					{
						if(dash_param->encoder == 0)
						{
							h265_read_frame(track_id, ptr, ptr + bytes, dash, f_pts);
							//LOGE("dash start%d .................................\n", dash_param->vStream);
						}
						else
						{
							LOGE("2.Error encoder - %d", dash_param->encoder);
						}
					}
					
					//printf("============== track_id - %d ===============\n", track_id);

					track_id++;
					FrameNumber++;
					vNoData = 0;
					//if(count_pthread > 0)
					//{
					//	//pthread_mutex_lock(&mtx);
					//	count_pthread--;
					//	if(!count_pthread)
					//	{
					//		pthread_cond_signal(&cond);
					//	}
					//	//pthread_mutex_unlock(&mtx);
					//}
				}
				else
				{
					
//#if defined(DEBUG_INFO_DUMP) 					
//					LOGE("7 : vNoData vFrame = %d, vfNumber = %d res - %d", vFrame, FrameNumber, res);
//#endif				
					wait_for_frame += 1000;
					if (wait_for_frame > 250000)
					{
						LOGE("MpegDash failed to get frame (%d)", res);
						done = 0;
						break;
					}
					
					//vNoData = 1;
					usleep(5000);
				}
			}
			freeSubsessionBuffer(vFrame);
			usleep(5000);
	}//while

exit_ptread:	
	
	if(dash->mpd != NULL) 
	{		
		dash_mpd_destroy(dash->mpd);
	}
 	delete dash;
//#if defined (DEBUG_LAST) 
	LOGE("---------------------------exit  from while ------------------------");
//#endif	
	pthread_exit(NULL);
}





