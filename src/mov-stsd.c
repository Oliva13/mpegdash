#include "mov-internal.h"
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

// stsd: Sample Description Box

static int mp4_read_extra(struct mov_t* mov, const struct mov_box_t* box)
{
	int r;
	uint64_t p1, p2;
	p1 = mov_buffer_tell(&mov->io);
	r = mov_reader_box(mov, box);
	p2 = mov_buffer_tell(&mov->io);
	mov_buffer_skip(&mov->io, box->size - (p2 - p1));
	return r;
}

/*
aligned(8) abstract class SampleEntry (unsigned int(32) format) 
	extends Box(format){ 
	const unsigned int(8)[6] reserved = 0; 
	unsigned int(16) data_reference_index; 
}
*/
static int mov_read_sample_entry(struct mov_t* mov, struct mov_box_t* box, uint16_t* data_reference_index)
{
	box->size = mov_buffer_r32(&mov->io);
	box->type = mov_buffer_r32(&mov->io);
	mov_buffer_skip(&mov->io, 6); // const unsigned int(8)[6] reserved = 0;
	*data_reference_index = (uint16_t)mov_buffer_r16(&mov->io); // ref [stsc] sample_description_index
	return 0;
}

/*
class AudioSampleEntry(codingname) extends SampleEntry (codingname){ 
	const unsigned int(32)[2] reserved = 0; 
	template unsigned int(16) channelcount = 2; 
	template unsigned int(16) samplesize = 16; 
	unsigned int(16) pre_defined = 0; 
	const unsigned int(16) reserved = 0 ; 
	template unsigned int(32) samplerate = { default samplerate of media}<<16; 
}
*/
static int mov_read_audio(struct mov_t* mov, struct mov_stsd_t* stsd)
{
	struct mov_box_t box;
	mov_read_sample_entry(mov, &box, &stsd->data_reference_index);
	stsd->object_type_indication = mov_tag_to_object(box.type);
	stsd->stream_type = MP4_STREAM_AUDIO;
	mov->track->tag = box.type;

#if 1
	// const unsigned int(32)[2] reserved = 0;
	mov_buffer_skip(&mov->io, 8);
#else
	mov_buffer_r16(&mov->io); /* version */
	mov_buffer_r16(&mov->io); /* revision level */
	mov_buffer_r32(&mov->io); /* vendor */
#endif

	stsd->u.audio.channelcount = (uint16_t)mov_buffer_r16(&mov->io);
	stsd->u.audio.samplesize = (uint16_t)mov_buffer_r16(&mov->io);

#if 1
	// unsigned int(16) pre_defined = 0; 
	// const unsigned int(16) reserved = 0 ;
	mov_buffer_skip(&mov->io, 4);
#else
	mov_buffer_r16(&mov->io); /* audio cid */
	mov_buffer_r16(&mov->io); /* packet size = 0 */
#endif

	stsd->u.audio.samplerate = mov_buffer_r32(&mov->io); // { default samplerate of media}<<16;

	// audio extra(avc1: ISO/IEC 14496-14:2003(E))
	box.size -= 36;
	return mp4_read_extra(mov, &box);
}

/*
class VisualSampleEntry(codingname) extends SampleEntry (codingname){ 
	unsigned int(16) pre_defined = 0; 
	const unsigned int(16) reserved = 0; 
	unsigned int(32)[3] pre_defined = 0; 
	unsigned int(16) width; 
	unsigned int(16) height; 
	template unsigned int(32) horizresolution = 0x00480000; // 72 dpi 
	template unsigned int(32) vertresolution = 0x00480000; // 72 dpi 
	const unsigned int(32) reserved = 0; 
	template unsigned int(16) frame_count = 1; 
	string[32] compressorname; 
	template unsigned int(16) depth = 0x0018; 
	int(16) pre_defined = -1; 
	// other boxes from derived specifications 
	CleanApertureBox clap; // optional 
	PixelAspectRatioBox pasp; // optional 
}
class AVCSampleEntry() extends VisualSampleEntry (avc1){
	AVCConfigurationBox config;
	MPEG4BitRateBox (); // optional
	MPEG4ExtensionDescriptorsBox (); // optional
}
class AVC2SampleEntry() extends VisualSampleEntry (avc2){
	AVCConfigurationBox avcconfig;
	MPEG4BitRateBox bitrate; // optional
	MPEG4ExtensionDescriptorsBox descr; // optional
	extra_boxes boxes; // optional
}
*/
static int mov_read_video(struct mov_t* mov, struct mov_stsd_t* stsd)
{
	struct mov_box_t box;
	mov_read_sample_entry(mov, &box, &stsd->data_reference_index);
	stsd->object_type_indication = mov_tag_to_object(box.type);
	stsd->stream_type = MP4_STREAM_VISUAL; 
	mov->track->tag = box.type;
#if 1
	 //unsigned int(16) pre_defined = 0; 
	 //const unsigned int(16) reserved = 0;
	 //unsigned int(32)[3] pre_defined = 0;
	mov_buffer_skip(&mov->io, 16);
#else
	mov_buffer_r16(&mov->io); /* version */
	mov_buffer_r16(&mov->io); /* revision level */
	mov_buffer_r32(&mov->io); /* vendor */
	mov_buffer_r32(&mov->io); /* temporal quality */
	mov_buffer_r32(&mov->io); /* spatial quality */
#endif
	stsd->u.visual.width = (uint16_t)mov_buffer_r16(&mov->io);
	stsd->u.visual.height = (uint16_t)mov_buffer_r16(&mov->io);
	stsd->u.visual.horizresolution = mov_buffer_r32(&mov->io); // 0x00480000 - 72 dpi
	stsd->u.visual.vertresolution = mov_buffer_r32(&mov->io); // 0x00480000 - 72 dpi
	// const unsigned int(32) reserved = 0;
	mov_buffer_r32(&mov->io); /* data size, always 0 */
	stsd->u.visual.frame_count = (uint16_t)mov_buffer_r16(&mov->io);

	//string[32] compressorname;
	//uint32_t len = mov_buffer_r8(&mov->io);
	//mov_buffer_skip(&mov->io, len);
	mov_buffer_skip(&mov->io, 32);

	stsd->u.visual.depth = (uint16_t)mov_buffer_r16(&mov->io);
	// int(16) pre_defined = -1;
	mov_buffer_skip(&mov->io, 2);

	// video extra(avc1: ISO/IEC 14496-15:2010(E))
	box.size -= 86;
	return mp4_read_extra(mov, &box);
}

static int mov_read_hint_sample_entry(struct mov_t* mov, struct mov_stsd_t* stsd)
{
	struct mov_box_t box;
	mov_read_sample_entry(mov, &box, &stsd->data_reference_index);
	mov_buffer_skip(&mov->io, box.size - 16);
	return mov_buffer_error(&mov->io);
}

static int mov_read_meta_sample_entry(struct mov_t* mov, struct mov_stsd_t* stsd)
{
	struct mov_box_t box;
	mov_read_sample_entry(mov, &box, &stsd->data_reference_index);
	mov_buffer_skip(&mov->io, box.size - 16);
	return mov_buffer_error(&mov->io);
}

int mov_read_stsd(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i, entry_count;
	struct mov_track_t* track = mov->track;

	mov_buffer_r8(&mov->io);
	mov_buffer_r24(&mov->io);
	entry_count = mov_buffer_r32(&mov->io);

	if (track->stsd_count < entry_count)
	{
		void* p = realloc(track->stsd, sizeof(struct mov_stsd_t) * entry_count);
		if (NULL == p) return ENOMEM;
		track->stsd = (struct mov_stsd_t*)p;
	}

	track->stsd_count = entry_count;
	for (i = 0; i < entry_count; i++)
	{
		if (MOV_AUDIO == track->handler_type)
		{
			mov_read_audio(mov, &track->stsd[i]);
		}
		else if (MOV_VIDEO == track->handler_type)
		{
			mov_read_video(mov, &track->stsd[i]);
		}
		else if (MOV_HINT == track->handler_type)
		{
			mov_read_hint_sample_entry(mov, &track->stsd[i]);
		}
		else if (MOV_META == track->handler_type)
		{
			mov_read_meta_sample_entry(mov, &track->stsd[i]);
		}
		else if (MOV_CLCP == track->handler_type)
		{
			mov_read_meta_sample_entry(mov, &track->stsd[i]);
		}
		else
		{
			assert(0); // ignore
			mov_read_meta_sample_entry(mov, &track->stsd[i]);
		}
	}

	(void)box;
	return mov_buffer_error(&mov->io);
}

//static int mov_write_h264(const struct mov_t* mov)
//{
//	size_t size;
//	uint64_t offset;
//	const struct mov_track_t* track = mov->track;
//
//	size = 8 /* Box */;
//
//	offset = mov_buffer_tell(&mov->io);
//	mov_buffer_w32(&mov->io, 0); /* size */
//	mov_buffer_w32(&mov->io, MOV_TAG('a', 'v', 'c', 'C'));
//
//	mov_write_size(mov, offset, size); /* update size */
//	return size;
//}

static int mov_write_video(const struct mov_t* mov, const struct mov_stsd_t* stsd)
{
	size_t size;
	uint64_t offset;
	assert(1 == stsd->data_reference_index);

	size = 8 /* Box */ + 8 /* SampleEntry */ + 70 /* VisualSampleEntry */;

	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_w32(&mov->io, mov->track->tag); // "h264"
#if defined(DEBUG_LAST) 
	printf("@@@@@@@@@@@@@@@@@@@@@ mov->track->tag - 0x%x\n", mov->track->tag);
#endif	
	mov_buffer_w32(&mov->io, 0); /* Reserved */
	mov_buffer_w16(&mov->io, 0); /* Reserved */
	mov_buffer_w16(&mov->io, stsd->data_reference_index); /* Data-reference index */

	mov_buffer_w16(&mov->io, 0); /* Reserved / Codec stream version */
	mov_buffer_w16(&mov->io, 0); /* Reserved / Codec stream revision (=0) */
	mov_buffer_w32(&mov->io, 0); /* Reserved */
	mov_buffer_w32(&mov->io, 0); /* Reserved */
	mov_buffer_w32(&mov->io, 0); /* Reserved */

	mov_buffer_w16(&mov->io, stsd->u.visual.width); /* Video width */
	mov_buffer_w16(&mov->io, stsd->u.visual.height); /* Video height */
	mov_buffer_w32(&mov->io, 0x00480000); /* Horizontal resolution 72dpi */
	mov_buffer_w32(&mov->io, 0x00480000); /* Vertical resolution 72dpi */
	mov_buffer_w32(&mov->io, 0); /* reserved / Data size (= 0) */
	mov_buffer_w16(&mov->io, 1); /* Frame count (= 1) */

	mov_buffer_w8(&mov->io, 0 /*strlen(compressor_name)*/); /* compressorname */
	mov_buffer_write(&mov->io, " ", 31); // fill empty

	mov_buffer_w16(&mov->io, 0x18); /* Reserved */
	mov_buffer_w16(&mov->io, 0xffff); /* Reserved */

	if(MOV_OBJECT_H264 == stsd->object_type_indication)
	{
		size += mov_write_avcc(mov);
	}
	else if(MOV_OBJECT_MP4V == stsd->object_type_indication)
	{
		size += mov_write_esds(mov);
	}	
	else if (MOV_OBJECT_HEVC == stsd->object_type_indication)
	{
		size += mov_write_hvcc(mov);
	}
	mov_write_size(mov, offset, size); /* update size */
	return size;
}

static int mov_write_audio(const struct mov_t* mov, const struct mov_stsd_t* stsd)
{
	size_t size;
	uint64_t offset;

	size = 8 /* Box */ + 8 /* SampleEntry */ + 20 /* AudioSampleEntry */;

	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_w32(&mov->io, mov->track->tag); // "mp4a"

	mov_buffer_w32(&mov->io, 0); /* Reserved */
	mov_buffer_w16(&mov->io, 0); /* Reserved */
	mov_buffer_w16(&mov->io, 1); /* Data-reference index */

	/* SoundDescription */
	mov_buffer_w16(&mov->io, 0); /* Version */
	mov_buffer_w16(&mov->io, 0); /* Revision level */
	mov_buffer_w32(&mov->io, 0); /* Reserved */

	mov_buffer_w16(&mov->io, stsd->u.audio.channelcount); /* channelcount */
	mov_buffer_w16(&mov->io, stsd->u.audio.samplesize); /* samplesize */

	mov_buffer_w16(&mov->io, 0); /* pre_defined */
	mov_buffer_w16(&mov->io, 0); /* reserved / packet size (= 0) */

	mov_buffer_w32(&mov->io, stsd->u.audio.samplerate); /* samplerate */

	if(MOV_OBJECT_AAC == stsd->object_type_indication)
		size += mov_write_esds(mov);

	mov_write_size(mov, offset, size); /* update size */
	return size;
}

size_t mov_write_stsd(const struct mov_t* mov)
{
	size_t i, size;
	uint64_t offset;
	const struct mov_track_t* track = mov->track;

	size = 12 /* full box */ + 4 /* entry count */;

	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "stsd", 4);
	mov_buffer_w32(&mov->io, 0); /* version & flags */
	mov_buffer_w32(&mov->io, track->stsd_count); /* entry count */

	for (i = 0; i < track->stsd_count; i++)
	{
		if (MOV_VIDEO == track->handler_type)
		{
			size += mov_write_video(mov, &track->stsd[i]);
		}
		else if (MOV_AUDIO == track->handler_type)
		{
			size += mov_write_audio(mov, &track->stsd[i]);
		}
		else
		{
			assert(0);
		}
	}

	mov_write_size(mov, offset, size); /* update size */
	return size;
}
