/*
 * H.265 video codec.
 * Copyright (c) 2013 openHEVC contributors
 * Copyright (c) 2013-2014 struktur AG, Dirk Farin <farin@struktur.de>
 *
 * This file is part of libde265.
 *
 * libde265 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libde265 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libde265.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SSE_DCT_H
#define SSE_DCT_H

#include <stddef.h>
#include <stdint.h>

#define TRANSFORM_4X4_ADD_8 transform_4x4_add_8_fallback
#define TRANSFORM_8X8_ADD_8 ff_hevc_transform_8x8_add_8_wasm
#define TRANSFORM_16X16_ADD_8 ff_hevc_transform_16x16_add_8_wasm
#define TRANSFORM_32X32_ADD_8 ff_hevc_transform_32x32_add_8_wasm

#define TRANSFORM_SKIP_8 ff_hevc_transform_skip_8_wasm

void ff_hevc_transform_skip_8_wasm(uint8_t *_dst, const int16_t *coeffs, ptrdiff_t _stride);
void ff_hevc_transform_4x4_luma_add_8_wasm(uint8_t *dst, const int16_t *coeffs, ptrdiff_t stride);
void ff_hevc_transform_4x4_add_8_wasm(uint8_t *dst, const int16_t *coeffs, ptrdiff_t stride);
void ff_hevc_transform_8x8_add_8_wasm(uint8_t *dst, const int16_t *coeffs, ptrdiff_t stride);
void ff_hevc_transform_16x16_add_8_wasm(uint8_t *dst, const int16_t *coeffs, ptrdiff_t stride);
void ff_hevc_transform_32x32_add_8_wasm(uint8_t *dst, const int16_t *coeffs, ptrdiff_t stride);

// fallback protos
void transform_4x4_add_8_fallback(uint8_t *dst, const int16_t *coeffs, ptrdiff_t stride);
void transform_bypass_rdpcm_v_fallback(int32_t *r, const int16_t *coeffs,int nT);
void transform_bypass_rdpcm_h_fallback(int32_t *r, const int16_t *coeffs,int nT);
void transform_bypass_fallback(int32_t *r, const int16_t *coeffs, int nT);
void transform_4x4_luma_add_8_fallback(uint8_t *dst, const int16_t *coeffs, ptrdiff_t stride);

void transform_idst_4x4_fallback(int32_t *dst, const int16_t *coeffs, int bdShift, int max_coeff_bits);
void transform_idct_4x4_fallback(int32_t *dst, const int16_t *coeffs, int bdShift, int max_coeff_bits);
void transform_idct_8x8_fallback(int32_t *dst, const int16_t *coeffs, int bdShift, int max_coeff_bits);
void transform_idct_16x16_fallback(int32_t *dst, const int16_t *coeffs, int bdShift, int max_coeff_bits);
void transform_idct_32x32_fallback(int32_t *dst, const int16_t *coeffs, int bdShift, int max_coeff_bits);

#endif
