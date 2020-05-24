#ifndef _RECORDER_H_
#define _RECORDER_H_

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

#include "boolean.h"
#include "FileWritingLibrary.h"
#include "avi_capture.h"
#include "dash-mpd.h"

#define VSTREAM0 0
#define VSTREAM1 1

//#define PREFIX_MDASH	"/mnt/mmcblk0p1/mpgdsh/"
#define PREFIX_MDASH	"/tvh/mpgdsh/"

#define NO_DATA_CMD		     "NO_DATA..."
#define CMD_LENGTH 	 		 11
#define LOCAL_PATH_LENGTH    256
#define MIN_SIZE_DISK_SPACE  8*1024 //Mb
#define FILE_NAME_LENGTH 	 64
#ifdef __cplusplus

typedef struct
{
    float   framerate;
    int     fd;
    int     width;
    int     height;
    int     vfNumber;
    int     afNumber;
    int     vStream;
    int     encoder;
    int     start_init; 
    //pthread_cond_t  cond;
	//pthread_mutex_t mtx;
	int     stream_active;
    char    stream_name[16];
    int64_t durf_pts;
    struct  ipc_unit *pmq;

}rec_video_t;



#include <string>

//extern std::atomic<bool> start_init;
struct dash_playlist_t
{
	std::string name;
	dash_mpd_t* mpd;
////////////////////////////////////////	
	int   video_stream;
	int64_t i_timestamp;
	int64_t var_timestamp;
	char mpd_name[N_NAME + 16];
	FILE *fp;
	char seg_name[N_NAME + 16];
///////////////////////////////////////	
	int64_t timestamp;
	int adapation_video;
	int adapation_audio;
	int count;
	int fname;
	uint32_t track_video;
	uint32_t track_audio;
	char playlist[256 * 1024];
	uint8_t packet[2 * 1024 * 1024];
	uint8_t s_buffer[2 * 1024 * 1024];
	rec_video_t *p_rec_video;
};

int init_h264_read_frame(void* param, unsigned long track_id, int frame_rate, int width, int height, const uint8_t* ptr, const uint8_t* end);
int h264_read_frame(unsigned long track_id, uint8_t* ptr, uint8_t* end, void* param_dash, int64_t pts_new);
int init_dash(const char* path, dash_playlist_t *dash, int stream);
void freeSubsessionBuffer(avi_frame_t *pSubBuffer);
void subscribe_change_param_video(int ch);

extern volatile sig_atomic_t pthread_start;
extern volatile sig_atomic_t done, count_pthread;
extern rec_video_t *param_video_stream_0; 
extern rec_video_t *param_video_stream_1;

int get_param(int num_stream, rec_video_t *mpd_param_video);
#define STREAM(p) strcat(strcat(strcpy(param,"stream"), num),":" p)
void del_all_mpd(void);
void del_mpd(void);
int del_all_dir_files(void);
void *thr_del_mpd(void *arg);
void *dash_server_worker(void* param);
//dash.h
void mp4_onread(void* param, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts);
void mp4_onvideo(void* param, uint32_t track, uint8_t object, int frame_rate, int width, int height, const void* extra, size_t bytes);
//h264.h
//h265.h
int init_h265_read_frame(void* param, unsigned long track_id, int frame_rate, int width, int height, const uint8_t* ptr, const uint8_t* end);
int h265_read_frame(unsigned long track_id, uint8_t* ptr, uint8_t* end, void* param_dash, int64_t pts_new);

size_t dash_mpd_playlist(dash_playlist_t* dash, struct dash_mpd_t* mpd, char* playlist, int stream, size_t bytes);
extern volatile sig_atomic_t done;

#endif

//extern int threadquit;
void deallocate(void **buffer);

#endif
