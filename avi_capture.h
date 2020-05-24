#ifndef _CAPTURE_AVI_H
#define _CAPTURE_AVI_H


//#define CHNL_AVI_H264    1
//#define CHNL_AVI_MJPG    2
#define CHNL_AVI_G711 	 3

#define AVI_AUDIO	0x20
#define AVI_PCM		(AVI_AUDIO|1)
#define AVI_G711	(AVI_AUDIO|2)
#define AVI_ADPCM	(AVI_AUDIO|3)

#define AVI_VIDEO	0x10
#define AVI_MJPEG	(AVI_VIDEO|1)
#define AVI_MPEG4	(AVI_VIDEO|2)
#define AVI_H264	(AVI_VIDEO|3)
#define AVI_H265	(AVI_VIDEO|4)

#define PREFIX	"/tvh/av/"
#define NUM_STREAMS	4

#define COUNT_FRAMES 199

#define PRINT(...) LOGW(__VA_ARGS__); {FILE *f = fopen("/tmp/update.ret", "w+t"); if (f) {fLOGW(f, __VA_ARGS__); fclose(f);}}

void get_status_avi(int idx);
int scan_dir(char *path);
int get_frame_num_audio(int audio_idx, int64_t video_pts);
int avi_put_frames_full(int vfNumber, int afNumber);
//int get_frame_num_video(int idx, int time_ms, avi_status_t *st_result, avi_status_t *max_result);
int capture_avi(int vfNumber, int afNumber, int64_t duraft_video_pts);
void *avi_get_frames(void *args);
void* RecordActionProc(void *arg);

#endif
