#ifndef SVEA_ASSETS_H
#define SVEA_ASSETS_H
#include "director.h"

typedef struct {
    uint32_t resource, type;
    uint16_t number;
    uint8_t name[256]; /* Original bytes: cast names may be MacRoman. */
    const uint8_t *specific;
    size_t specific_size;
} SrCast;
typedef struct {
    uint16_t pitch, width, height, depth;
    int16_t top, left, reg_x, reg_y, palette;
} SrBitmap;
typedef struct {
    const uint8_t *bytes, *styles;
    uint32_t length;
    uint16_t style_count;
} SrText;
typedef struct {
    uint32_t start;
    uint16_t height, ascent, font_id, size, red, green, blue;
    uint8_t face;
} SrTextStyle;
typedef struct {
    uint8_t border, gutter, box_shadow, type, text_shadow, flags;
    int16_t alignment, scroll, top, left, bottom, right;
    uint16_t background[3], max_height, text_height;
} SrTextBox;
typedef struct {
    uint16_t platform, id;
    const uint8_t *name;
    uint32_t name_length;
} SrFontMapping;
typedef struct {
    const uint8_t *samples;
    uint32_t frames, rate_fixed, loop_start, loop_end;
    uint16_t channels, bits;
} SrSound;
/* Mac snd format 2, one inline bufferCmd, standard/extended PCM header.
   8-bit samples are unsigned; 16-bit samples are signed big-endian. */
const char *sr_sound_open(const uint8_t *data,size_t size,SrSound *sound);

const char *sr_find(const SrArchive *a, const char tag[4], SrResource *r);
const char *sr_cast_range(const SrArchive *a, uint16_t *first, uint32_t *count);
const char *sr_cast(const SrArchive *a, uint16_t number, SrCast *cast);
const char *sr_child(const SrArchive *a, uint32_t parent, const char tag[4], SrResource *r);
const char *sr_bitmap_info(const SrCast *cast, SrBitmap *bitmap);
/* D5 STXT and text CASt metadata. Strings retain their original byte encoding. */
const char *sr_text_open(const uint8_t *data,size_t size,SrText *text);
const char *sr_text_style(const SrText *text,uint16_t index,SrTextStyle *style);
const char *sr_text_box(const SrCast *cast,SrTextBox *box);
const char *sr_fontmap_count(const uint8_t *data,size_t size,uint32_t *count);
const char *sr_fontmap_entry(const uint8_t *data,size_t size,uint32_t index,SrFontMapping *font);
/* Decode Director RLE to exact pitch*height bytes. 0x80 repeats 129 bytes. */
const char *sr_bitd_unpack(const uint8_t *src, size_t size, uint8_t *dst, size_t expected);
void sr_mac_palette(uint8_t rgb[768]);
const char *sr_bitmap_rgba(const SrBitmap *b, const uint8_t *raw, size_t raw_size,
                          int compressed, uint8_t *rgba, size_t rgba_size);
/* Decode original D5 score deltas. These are asset records, not a script VM. */
#define SR_SCORE_BYTES (48 + 48 * 24)
const char *sr_score_frame(const uint8_t *data, size_t size, uint32_t frame,
                           uint8_t state[SR_SCORE_BYTES], uint32_t *count);
#endif
