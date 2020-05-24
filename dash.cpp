#include "dash-mpd.h"
#include "dash-proto.h"
#include "mov-reader.h"
#include "mov-format.h"
#include "fmp4-writer.h"
#include "mpeg4-avc.h"
#include "path.h"
#include <unordered_map>
#include <sys/time.h>
#include <signal.h>
#include <iostream>
#include <string>
#include <cstring>
#include <functional>
#include <cassert>
#include <cinttypes>
#include <atomic>
#include "FileWritingLibrary.h"
#include "recorder.h"
#include "mpdutils.h"

void mp4_onvideo(void* param, uint32_t track, uint8_t object, int frame_rate, int width, int height, const void* extra, size_t bytes)
{
	dash_playlist_t* dash = (dash_playlist_t*)param;
	dash->track_video = track;
	dash->adapation_video = dash_mpd_add_video_adapation_set(dash->mpd, dash->name.c_str(), object, frame_rate, width, height, extra, bytes);
}

static void mp4_onaudio(void* param, uint32_t track, uint8_t object, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes)
{
	dash_playlist_t* dash = (dash_playlist_t*)param;
	dash->track_audio = track;
	dash->adapation_audio = dash_mpd_add_audio_adapation_set(dash->mpd, dash->name.c_str(), object, channel_count, bit_per_sample, sample_rate, extra, bytes);
}

void mp4_onread(void* param, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts)
{
	dash_playlist_t* dash = (dash_playlist_t*)param;
	int64_t  i_timestamp = dash->i_timestamp, var_timestamp = dash->var_timestamp;
	int64_t	 difftimestamp = 0;
	dash->timestamp = dts;
	
	if (dash->track_video == track)
	{
			//printf("keyframe1 - %d\n", (0x1f & ((uint8_t*)buffer)[4]));
					
			uint8_t tmp = (0x1f & ((uint8_t*)buffer)[4]);			
  		    
			bool keyframe = 0;
			
			if(tmp == 14 || tmp == 7)
			{
				keyframe = 1;		
			}
			
			if(keyframe == 1)
			{
				if(dash->i_timestamp != 0)
				{
					difftimestamp = pts - dash->i_timestamp; 
				}
				dash->i_timestamp = pts;
			
			//	//printf("Start point ========== keyframe2 - %d\n", keyframe);
			
			//	//... difftimestamp - %lld\n", keyframe, difftimestamp);
			//	
				if(difftimestamp < 600) 
				{
					if(dash->var_timestamp < 600) 
					{
						dash->var_timestamp += difftimestamp; 
						keyframe = 0;
			//			//printf("var_timestamp1 - %lld\n", var_timestamp);
					}
					else
					{
						dash->var_timestamp = difftimestamp;
						//printf("dash->var_timestamp2 - %lld\n", var_timestamp);
					}
				}				
			}
			
			//printf("dash->var_timestamp - %lld\, keyframe - %d\n", dash->var_timestamp, keyframe);
			
			dash_mpd_input(dash->mpd, dash->adapation_video, buffer, bytes, pts, dts, keyframe ? MOV_AV_FLAG_KEYFREAME : 0);
	}
	else
	{
		if (dash->track_audio == track)
		{
			dash_mpd_input(dash->mpd, dash->adapation_audio, buffer, bytes, pts, dts, 0);
		}
		else
		{
			printf("dash->track_video - %d, track - %d\n", dash->track_video, track);
			assert(0);
		}
	}
}

int dash_mpd_onsegment(void* param, int /*track*/, const void* data, size_t bytes, int64_t /*pts*/, int64_t /*dts*/, int64_t /*duration*/, const char* name)
{
	char newname[128];
	
	sprintf(newname, PREFIX_MDASH"%s", name);
		
	FILE* fp = fopen(newname, "wb");
	if (fp == NULL)
	{
		printf("Error: dash_mpd_onsegment : fopen - %s\n", newname);
		return 0;
	}
	
	fwrite(data, 1, bytes, fp);
	
	fclose(fp);
	
	dash_playlist_t* dash = (dash_playlist_t*)param;
	
#if defined(DEBUG_INFO_DUMP)		
	printf("seg_handler : dash_mpd_onsegment : name - %s, stream - %d\n", name, dash->video_stream);
#endif	
	dash_mpd_playlist(dash, dash->mpd, dash->playlist, dash->video_stream, sizeof(dash->playlist));
	
#if defined(DEBUG_INFO_DUMP)		
	printf("dash_mpd_onsegment exit\n");
#endif		
	return 0;
}

//char param[64], num[2], roi[2];
//sprintf(num, "%d", i);
//cfg_get_int(STREAM("enabled"), &video[i].enabled);
//cfg_get_int(STREAM("encoder"), (int *)&video[i].encoder);
//cfg_get_int(STREAM("resolution"), &video[i].resolution);
//cfg_get_float(STREAM("framerate"), &video[i].framerate);
//cfg_get_int(STREAM("quality"), &video[i].quality);
//cfg_get_int(STREAM("bitrate"), &video[i].bitrate);
//cfg_get_int(STREAM("ratecontrol"), (int *)&video[i].ratectrl);
//cfg_get_int(STREAM("goplength"), &video[i].gop);

int init_dash(const char* path, dash_playlist_t *dash, int stream)
{
	//struct dash_mpd_notify_t notify;
#if defined(DEBUG_INFO_DUMP)			
	printf("init_dash: live: %s stream - %d\n", path, stream);
#endif	
	char* name = (char*)path_basename(path);
	
	dash->name = name;
	dash->video_stream = stream;
	dash->count = 0;
	dash->fname = 1;
	dash->mpd = dash_mpd_create(DASH_DYNAMIC, dash_mpd_onsegment, dash); // !!!! dash->mpd
	//printf("exit - init_dash, stream - %d\n", stream);
}





