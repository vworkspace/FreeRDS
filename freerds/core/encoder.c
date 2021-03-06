/**
 * FreeRDS: FreeRDP Remote Desktop Services (RDS)
 * Bitmap Encoder
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "core.h"

#include "encoder.h"

#include <winpr/crt.h>

int freerds_encoder_create_frame_id(rdsEncoder* encoder)
{
	UINT32 frameId;
	int inFlightFrames;
	SURFACE_FRAME* frame;

	inFlightFrames = ListDictionary_Count(encoder->frameList);

	if (inFlightFrames > encoder->frameAck)
	{
		encoder->fps = (100 / (inFlightFrames + 1) * encoder->maxFps) / 100;
	}
	else
	{
		encoder->fps += 2;

		if (encoder->fps > encoder->maxFps)
			encoder->fps = encoder->maxFps;
	}

	if (encoder->fps < 1)
		encoder->fps = 1;

	frame = (SURFACE_FRAME*) malloc(sizeof(SURFACE_FRAME));

	if (!frame)
		return -1;

	frameId = frame->frameId = ++encoder->frameId;
	ListDictionary_Add(encoder->frameList, (void*) (size_t) frame->frameId, frame);

	return (int) frame->frameId;
}

int freerds_encoder_init_grid(rdsEncoder* encoder)
{
	int i, j, k;
	int tileSize;
	int tileCount;

	encoder->gridWidth = ((encoder->width + (encoder->maxTileWidth - 1)) / encoder->maxTileWidth);
	encoder->gridHeight = ((encoder->height + (encoder->maxTileHeight - 1)) / encoder->maxTileHeight);

	tileSize = encoder->maxTileWidth * encoder->maxTileHeight * 4;
	tileCount = encoder->gridWidth * encoder->gridHeight;

	encoder->gridBuffer = (BYTE*) malloc(tileSize * tileCount);

	if (!encoder->gridBuffer)
		return -1;

	encoder->grid = (BYTE**) malloc(tileCount * sizeof(BYTE*));

	if (!encoder->grid)
		return -1;

	for (i = 0; i < encoder->gridHeight; i++)
	{
		for (j = 0; j < encoder->gridWidth; j++)
		{
			k = (i * encoder->gridWidth) + j;
			encoder->grid[k] = &(encoder->gridBuffer[k * tileSize]);
		}
	}

	return 0;
}

int freerds_encoder_uninit_grid(rdsEncoder* encoder)
{
	if (encoder->gridBuffer)
	{
		free(encoder->gridBuffer);
		encoder->gridBuffer = NULL;
	}

	if (encoder->grid)
	{
		free(encoder->grid);
		encoder->grid = NULL;
	}

	encoder->gridWidth = 0;
	encoder->gridHeight = 0;

	return 0;
}

int freerds_encoder_init_rfx(rdsEncoder* encoder)
{
	rdpSettings* settings = encoder->connection->settings;

	if (!encoder->rfx)
		encoder->rfx = rfx_context_new(TRUE);

	if (!encoder->rfx)
		return -1;

	encoder->rfx->mode = RLGR3;
	encoder->rfx->width = encoder->width;
	encoder->rfx->height = encoder->height;

	rfx_context_set_pixel_format(encoder->rfx, RDP_PIXEL_FORMAT_B8G8R8A8);

	if (!encoder->frameList)
	{
		encoder->fps = 16;
		encoder->maxFps = 32;
		encoder->frameId = 0;
		encoder->frameList = ListDictionary_New(TRUE);
		encoder->frameAck = settings->SurfaceFrameMarkerEnabled;
	}

	encoder->codecs |= FREERDP_CODEC_REMOTEFX;

	return 1;
}

int freerds_encoder_init_nsc(rdsEncoder* encoder)
{
	rdpSettings* settings = encoder->connection->settings;

	if (!encoder->nsc)
		encoder->nsc = nsc_context_new();

	if (!encoder->nsc)
		return -1;

	nsc_context_set_pixel_format(encoder->nsc, RDP_PIXEL_FORMAT_B8G8R8A8);

	if (!encoder->frameList)
	{
		encoder->fps = 16;
		encoder->maxFps = 32;
		encoder->frameId = 0;
		encoder->frameList = ListDictionary_New(TRUE);
		encoder->frameAck = settings->SurfaceFrameMarkerEnabled;
	}

	encoder->nsc->ColorLossLevel = settings->NSCodecColorLossLevel;
	encoder->nsc->ChromaSubsamplingLevel = settings->NSCodecAllowSubsampling ? 1 : 0;
	encoder->nsc->DynamicColorFidelity = settings->NSCodecAllowDynamicColorFidelity;

	encoder->codecs |= FREERDP_CODEC_NSCODEC;

	return 1;
}

int freerds_encoder_init_planar(rdsEncoder* encoder)
{
	DWORD planarFlags = 0;
	rdpSettings* settings = encoder->connection->settings;

	if (settings->DrawAllowSkipAlpha)
		planarFlags |= PLANAR_FORMAT_HEADER_NA;

	planarFlags |= PLANAR_FORMAT_HEADER_RLE;

	if (!encoder->planar)
	{
		encoder->planar = freerdp_bitmap_planar_context_new(planarFlags,
				encoder->maxTileWidth, encoder->maxTileHeight);
	}

	if (!encoder->planar)
		return -1;

	encoder->codecs |= FREERDP_CODEC_PLANAR;

	return 1;
}

int freerds_encoder_init_interleaved(rdsEncoder* encoder)
{
	if (!encoder->interleaved)
		encoder->interleaved = bitmap_interleaved_context_new(TRUE);

	if (!encoder->interleaved)
		return -1;

	encoder->codecs |= FREERDP_CODEC_INTERLEAVED;

	return 1;
}

int freerds_encoder_init(rdsEncoder* encoder)
{
	encoder->maxTileWidth = 64;
	encoder->maxTileHeight = 64;

	freerds_encoder_init_grid(encoder);

	if (!encoder->bs)
		encoder->bs = Stream_New(NULL, encoder->maxTileWidth * encoder->maxTileHeight * 4);

	if (!encoder->bs)
		return -1;

	return 1;
}

int freerds_encoder_uninit_rfx(rdsEncoder* encoder)
{
	encoder->frameId = 0;

	if (encoder->rfx)
	{
		rfx_context_free(encoder->rfx);
		encoder->rfx = NULL;
	}

	if (encoder->frameList)
	{
		ListDictionary_Free(encoder->frameList);
		encoder->frameList = NULL;
	}

	encoder->codecs &= ~FREERDP_CODEC_REMOTEFX;

	return 1;
}

int freerds_encoder_uninit_nsc(rdsEncoder* encoder)
{
	if (encoder->nsc)
	{
		nsc_context_free(encoder->nsc);
		encoder->nsc = NULL;
	}

	if (encoder->frameList)
	{
		ListDictionary_Free(encoder->frameList);
		encoder->frameList = NULL;
	}

	encoder->codecs &= ~FREERDP_CODEC_NSCODEC;

	return 1;
}

int freerds_encoder_uninit_planar(rdsEncoder* encoder)
{
	if (encoder->planar)
	{
		freerdp_bitmap_planar_context_free(encoder->planar);
		encoder->planar = NULL;
	}

	encoder->codecs &= ~FREERDP_CODEC_PLANAR;

	return 1;
}

int freerds_encoder_uninit_interleaved(rdsEncoder* encoder)
{
	if (encoder->interleaved)
	{
		bitmap_interleaved_context_free(encoder->interleaved);
		encoder->interleaved = NULL;
	}

	encoder->codecs &= ~FREERDP_CODEC_INTERLEAVED;

	return 1;
}

int freerds_encoder_uninit(rdsEncoder* encoder)
{
	freerds_encoder_uninit_grid(encoder);

	if (encoder->bs)
	{
		Stream_Free(encoder->bs, TRUE);
		encoder->bs = NULL;
	}

	if (encoder->codecs & FREERDP_CODEC_REMOTEFX)
	{
		freerds_encoder_uninit_rfx(encoder);
	}

	if (encoder->codecs & FREERDP_CODEC_NSCODEC)
	{
		freerds_encoder_uninit_nsc(encoder);
	}

	if (encoder->codecs & FREERDP_CODEC_PLANAR)
	{
		freerds_encoder_uninit_planar(encoder);
	}

	if (encoder->codecs & FREERDP_CODEC_INTERLEAVED)
	{
		freerds_encoder_uninit_interleaved(encoder);
	}

	return 1;
}

int freerds_encoder_reset(rdsEncoder* encoder)
{
	int status;
	UINT32 codecs = encoder->codecs;

	status = freerds_encoder_uninit(encoder);

	if (status < 0)
		return -1;

	status = freerds_encoder_init(encoder);

	if (status < 0)
		return -1;

	status = freerds_encoder_prepare(encoder, codecs);

	if (status < 0)
		return -1;

	return 1;
}

int freerds_encoder_prepare(rdsEncoder* encoder, UINT32 codecs)
{
	int status;

	if ((codecs & FREERDP_CODEC_REMOTEFX) && !(encoder->codecs & FREERDP_CODEC_REMOTEFX))
	{
		status = freerds_encoder_init_rfx(encoder);

		if (status < 0)
			return -1;
	}

	if ((codecs & FREERDP_CODEC_NSCODEC) && !(encoder->codecs & FREERDP_CODEC_NSCODEC))
	{
		status = freerds_encoder_init_nsc(encoder);

		if (status < 0)
			return -1;
	}

	if ((codecs & FREERDP_CODEC_PLANAR) && !(encoder->codecs & FREERDP_CODEC_PLANAR))
	{
		status = freerds_encoder_init_planar(encoder);

		if (status < 0)
			return -1;
	}

	if ((codecs & FREERDP_CODEC_INTERLEAVED) && !(encoder->codecs & FREERDP_CODEC_INTERLEAVED))
	{
		status = freerds_encoder_init_interleaved(encoder);

		if (status < 0)
			return -1;
	}

	return 1;
}

rdsEncoder* freerds_encoder_new(rdsConnection* connection, int width, int height, int bpp)
{
	rdsEncoder* encoder;

	encoder = (rdsEncoder*) calloc(1, sizeof(rdsEncoder));

	if (!encoder)
		return NULL;

	encoder->connection = connection;

	encoder->fps = 16;
	encoder->maxFps = 32;

	encoder->width = width;
	encoder->height = height;

	if (freerds_encoder_init(encoder) < 0)
	{
		free (encoder);
		return NULL;
	}

	return encoder;
}

void freerds_encoder_free(rdsEncoder* encoder)
{
	if (!encoder)
		return;

	freerds_encoder_uninit(encoder);

	free(encoder);
}
