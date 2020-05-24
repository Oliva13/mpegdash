#include "mov-internal.h"
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

// extra_data: ISO/IEC 14496-15:2017 HEVCDecoderConfigurationRecord

int mov_read_hvcc(struct mov_t* mov, const struct mov_box_t* box)
{
	struct mov_track_t* track = mov->track;
	if (track->extra_data_size < box->size)
	{
		void* p = realloc(track->extra_data, (size_t)box->size);
		if (NULL == p) return ENOMEM;
		track->extra_data = p;
	}

	mov_buffer_read(&mov->io, track->extra_data, (size_t)box->size);
	track->extra_data_size = (size_t)box->size;
	return mov_buffer_error(&mov->io);
}

size_t mov_write_hvcc(const struct mov_t* mov)
{
	const struct mov_track_t* track = mov->track;
	mov_buffer_w32(&mov->io, track->extra_data_size + 8); /* size */
	mov_buffer_write(&mov->io, "hvcC", 4);
	if (track->extra_data_size > 0)
		mov_buffer_write(&mov->io, track->extra_data, track->extra_data_size);
	return track->extra_data_size + 8;
}

//size_t mov_write_hvcc_seg(const struct mov_t* mov)
//{
//	const struct mov_track_t* track = mov->track;
//	int i = 0;
//	
//	if (track->extra_data_size > 0)
//	{
//		uint8_t* ptr = (track->extra_data + 24);
//		size_t size_ptr = (track->extra_data_size - 24); // To do in 24 ???
//		mov_buffer_write(&mov->io, ptr, size_ptr);
//		//for(i = 0; i < size_ptr; i++)
//		//{
//		//	printf("extra_data[%d] = 0x%x\n", i, ptr[i]);
//		//}
//	}
//	return track->extra_data_size + 8;
//}

size_t mov_write_hvcc_seg(const struct mov_t* mov)
{
	const struct mov_track_t* track = mov->track;
	int i = 0, z = 0;
	
	uint8_t extra_data_hev1[72] = { 0x00,0x00,0x00,0x18,0x40,0x01,0x0c,0x01,0xff,0xff,0x01,0x60,0x00,0x00,0x03,0x00,0xb0,0x00,
									0x00,0x03,0x00,0x00,0x03,0x00,0x5d,0xaa,0x02,0x40,0x00,0x00,0x00,0x1d,0x42,0x01,0x01,0x01,
									0x60,0x00,0x00,0x03,0x00,0xb0,0x00,0x00,0x03,0x00,0x00,0x03,0x00,0x5d,0xa0,0x02,0x80,0x80,
								  0x2d,0x16,0x36,0xaa,0x49,0x32,0xf9,0x00,0x00,0x00,0x07,0x44,0x01,0xc0,0xf2,0xf0,0x3c,0x90};
	
	
	//if (track->extra_data_size > 0)
	//{
	//uint8_t* ptr = (track->extra_data + 24);
	//size_t size_ptr = (track->extra_data_size - 24); // To do in 24 ???
	mov_buffer_write(&mov->io,&extra_data_hev1, 72);
	//	for(i = 0; i < size_ptr; i++)
	//	{
	//		printf("0x%x,", ptr[i]);
	//		if(z == 30)
	//		{
	//			printf("\n");
	//			z = 0;
	//		}
	//		z++;
	//	}
	//}
	//printf("\n");
	return track->extra_data_size + 8;
}


