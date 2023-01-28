/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - encoder/backend.h                                       *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2023 Jacky Guo                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#ifndef M64P_BACKENDS_ENCODER_BACKEND_H
#define M64P_BACKENDS_ENCODER_BACKEND_H

#include "api/m64p_types.h"

struct encoder_backend_interface {
  /**
   * Backend format, as in the enum.
   */
  const m64p_encoder_format format;
  
  /**
   * Initialize a backend instance. The handle is returned via obj.
   * This must be freed later, either by discard() or save().
   */
  m64p_error (*init)(void** self, const char* path, intptr_t hints[]);
  
  /**
   * Called every VI to dump a frame.
   */
  m64p_error (*encode_vi)(void* self, void* frame);
  /**
   * Frees the encoder backend, discarding all encoded data.
   */
  void (*discard)(void* self);
  /**
   * Frees the encoder backend, saving the encoded data to the path provided at init().
   */
  m64p_error (*save)(void* self);
};

/* collection of available encoder backends */
extern const struct encoder_backend_interface* g_encoder_backend_interfaces[];
/* helper function to find backend by format */
const struct encoder_backend_interface* get_video_capture_backend(m64p_encoder_format name);

#endif