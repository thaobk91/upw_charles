/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef __CODEC_H__
#define __CODEC_H__


#include <pb_encode.h>
#include <pb_decode.h>
#include "src/protobuf/msg.pb.h"
#include "utils.h"

bool encode_message(struct shadow_update_t update, uint8_t *buffer, size_t buffer_size, size_t *message_length);
bool decode_message(uint8_t *buffer, size_t message_length);

#endif /* __CODEC_H__ */
