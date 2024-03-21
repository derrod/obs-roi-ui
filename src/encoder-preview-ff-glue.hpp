#pragma once

#include <obs-module.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

bool CreateCodecContext(AVCodecContext **ctx, obs_encoder_t *enc);
bool SendExtraData(AVCodecContext *ctx, obs_encoder_t *enc);
bool SendPacket(AVCodecContext *ctx, const encoder_packet *pkt);
bool ReceiveFrame(AVCodecContext *ctx, AVFrame *frame);

void AVFrameToSourceFrame(obs_source_frame *dst, AVFrame *src,
			  AVRational time_base);
