#ifndef _dash_mpd_h_
#define _dash_mpd_h_

#include <stddef.h>
#include <stdint.h>
#include "list.h"
#include "fmp4-writer.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define N_TRACK 8
#define N_NAME 128
#define N_COUNT 2

#define N_SEGMENT (1 * 1024 * 1024)
#define N_FILESIZE (100 * 1024 * 1024) // 100M

#define MAX(a, b) ((a) > (b) ? (a) : (b))

struct dash_segment_t
{
	struct list_head link;
	int64_t timestamp;
	int64_t duration;
};

struct dash_adaptation_set_t
{
	fmp4_writer_t* fmp4;
	char prefix[N_NAME];

	uint8_t* ptr;
	size_t bytes;
	size_t capacity;
	size_t offset;
	size_t maxsize; // max bytes per mp4 file

	int64_t pts;
	int64_t dts;
	int64_t dts_last;
	int64_t raw_bytes;
	int bitrate;
	int track; // MP4 track id
	int setid; // dash adapation set id
	
	int seq;
	uint8_t object;

	union
	{
		struct
		{
			int width;
			int height;
			int frame_rate;
			struct
			{
				uint8_t profile;
				uint8_t compatibility;
				uint8_t level;
			} avc;
			
			struct
			{
				uint8_t  configurationVersion;	// 1-only
				uint8_t  general_profile_space;	// 2bit,[0,3]
				uint8_t  general_tier_flag;		// 1bit,[0,1]
				uint8_t  general_profile_idc;	// 5bit,[0,31]
				uint32_t general_profile_compatibility_flags;
				uint64_t general_constraint_indicator_flags;
				uint8_t  general_level_idc;
			} hevc;

		} video;

		struct
		{
			uint8_t profile; // AAC profile
			int channel;
			int sample_bit;
			int sample_rate;
		} audio;
	} u;

	size_t count;
	struct list_head root; // segments
};

typedef struct dash_mpd_t dash_mpd_t;

typedef int (*dash_mpd_segment)(void* param, int adapation, const void* data, size_t bytes, int64_t pts, int64_t dts, int64_t duration, const char* name);

struct dash_mpd_t
{
	int flags;
	time_t time;
	int64_t duration;
	int64_t max_segment_duration;

	dash_mpd_segment handler;
	void* param;

	int count; // adaptation set count
	struct dash_adaptation_set_t tracks[N_TRACK];
};

dash_mpd_t* dash_mpd_create(int flags, dash_mpd_segment handler, void* param);
void dash_mpd_destroy(dash_mpd_t* mpd);

/// @param[in] prefix dash adapation set name prefix
/// @return >=0-adapation id, <0-error
int dash_mpd_add_video_adapation_set(dash_mpd_t* mpd, const char* prefix, uint8_t object, int frame_rate, int width, int height, const void* extra_data, size_t extra_data_size);
int dash_mpd_add_audio_adapation_set(dash_mpd_t* mpd, const char* prefix, uint8_t object, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size);

/// @param[in] adapation create by dash_mpd_add_video_adapation_set/dash_mpd_add_audio_adapation_set
int dash_mpd_input(dash_mpd_t* mpd, int adapation, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags);


#ifdef __cplusplus
}
#endif
#endif /* !_dash_mpd_h_ */
