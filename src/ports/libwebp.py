# libwebp.py — custom Emscripten port, Phase 8c
#
# Builds a decoder-only libwebp (no encoder, no SIMD, no mux/demux).
# Provides WebPDecodeRGBA() for direct RGBA pixel access in Mod.cpp.
#
# Usage: --use-port=${CMAKE_CURRENT_SOURCE_DIR}/ports/libwebp.py
#
# License: BSD-style (see COPYING in the libwebp source tree).

import os

TAG  = '1.4.0'
HASH = '1217363fbb5c860b17c2ba4612f240f121c74ced6e3e58e8aa61252a9022f59893c5874bfa433cc50a7e65bac1ae2bfa99fa2cede070183b7a467f148cebb0bd'


def needed(settings):
    return True


def get(ports, settings, shared):
    ports.fetch_project(
        'libwebp',
        f'https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-{TAG}.tar.gz',
        sha512hash=HASH,
    )

    def create(final):
        src_dir = ports.get_dir('libwebp', f'libwebp-{TAG}')

        # Expose <webp/decode.h> (and friends) under the standard include path.
        ports.install_headers(os.path.join(src_dir, 'src', 'webp'), target='webp')

        # Decoder-only source list — no encoder, no SIMD variants, no mux/demux.
        srcs = [
            # Core decoder
            'src/dec/alpha_dec.c',
            'src/dec/buffer_dec.c',
            'src/dec/frame_dec.c',
            'src/dec/idec_dec.c',
            'src/dec/io_dec.c',
            'src/dec/quant_dec.c',
            'src/dec/tree_dec.c',
            'src/dec/vp8_dec.c',
            'src/dec/vp8l_dec.c',
            'src/dec/webp_dec.c',
            # DSP — scalar only (SIMD variants compiled-in only when CPU feature flags match;
            # WASM has no native SSE2/NEON so the scalar path is always taken at runtime).
            'src/dsp/alpha_processing.c',
            'src/dsp/cpu.c',
            'src/dsp/dec.c',
            'src/dsp/dec_clip_tables.c',
            'src/dsp/filters.c',
            'src/dsp/lossless.c',
            'src/dsp/rescaler.c',
            'src/dsp/upsampling.c',
            'src/dsp/yuv.c',
            # Utils — decode path only
            'src/utils/bit_reader_utils.c',
            'src/utils/color_cache_utils.c',
            'src/utils/filters_utils.c',
            'src/utils/huffman_utils.c',
            'src/utils/palette.c',
            'src/utils/quant_levels_dec_utils.c',
            'src/utils/random_utils.c',
            'src/utils/rescaler_utils.c',
            'src/utils/thread_utils.c',
            'src/utils/utils.c',
        ]

        ports.build_port(src_dir, final, 'libwebp',
                         srcs=srcs,
                         flags=['-I' + src_dir])

    return [shared.cache.get_lib('libwebpdecoder.a', create, what='port')]


def clear(ports, settings, shared):
    shared.cache.erase_lib('libwebpdecoder.a')


def show():
    return f'libwebp {TAG} decoder-only (--use-port=…/ports/libwebp.py; BSD license)'
