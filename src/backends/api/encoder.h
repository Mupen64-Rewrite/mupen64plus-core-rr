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
#include <stdbool.h>

struct encoder_backend_interface {
    /**
     * Initialize a backend instance. The handle is returned via obj.
     * This must be freed later, either by discard() or save().
     * If this function raises an error, the value will be deallocated.
     */
    m64p_error (*init)(void** self, const char* path, m64p_encoder_format fmt);

    /**
     * Called every VI to encode a frame. This should call gfx.readScreen.
     */
    m64p_error (*push_video)(void* self);
    
    /**
     * Called to set the audio sample rate.
     */
    m64p_error (*set_sample_rate)(void* self, unsigned int rate);
    
    /**
     * Called to encode audio samples.
     */
    m64p_error (*push_audio)(void* self, void* samples, size_t len);

    /**
     * Frees the encoder backend. If discard is true, then it should not save the file.
     */
    void (*free)(void* self, bool discard);
};

#endif