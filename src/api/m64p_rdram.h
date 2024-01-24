/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-core - m64p_rdram.h                                       *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2024 Jacky Guo                                          *
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

/* This header file defines typedefs for function pointers to RDRAM
 * functions.
 */

#ifndef M64P_RDRAM_H
#define M64P_RDRAM_H
#include <stdint.h>
#include "api/m64p_types.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifndef M64P_RDRAM_PROTOTYPES
#define M64P_API_FN(RT, name, ...) \
  typedef RT (*ptr_##name)(__VA_ARGS__)
#else
#define M64P_API_FN(RT, name, ...) \
  typedef RT (*ptr_##name)(__VA_ARGS__); \
  EXPORT RT CALL name(__VA_ARGS__)
#endif

M64P_API_FN(m64p_error, RDRAM_ReadAligned, uint32_t addr, uint32_t* value);
M64P_API_FN(m64p_error, RDRAM_WriteAligned, uint32_t addr, uint32_t value, uint32_t mask);
M64P_API_FN(uint32_t*, RDRAM_GetMemBase);

#undef M64P_API_FN

#ifdef __cplusplus
}
#endif
#endif