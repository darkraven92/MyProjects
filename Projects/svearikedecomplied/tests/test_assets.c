#include "assets.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    uint8_t out[256];
    const uint8_t mixed[]={2,1,2,3,253,9};
    const uint8_t expected[]={1,2,3,9,9,9,9};
    CHECK(!sr_bitd_unpack(mixed,sizeof mixed,out,7));
    CHECK(!memcmp(out,expected,7));
    const uint8_t repeat[]={128,42}, truncated[]={3,1,2};
    CHECK(!sr_bitd_unpack(repeat,2,out,129));
    for(int i=0;i<129;++i) CHECK(out[i]==42);
    CHECK(sr_bitd_unpack(repeat,2,out,128));
    CHECK(sr_bitd_unpack(truncated,3,out,4));
    CHECK(sr_bitd_unpack(repeat,1,out,129));
    CHECK(!sr_bitd_unpack(expected,7,out,7) && !memcmp(out,expected,7));
    uint8_t palette[768]; sr_mac_palette(palette);
    CHECK(palette[0]==255 && palette[5]==204 && palette[767]==0);
    CHECK(palette[215*3]==238 && palette[215*3+1]==0);
    SrBitmap b={2,2,1,8,0,0,0,0,0};
    const uint8_t raw[]={0,255}; uint8_t rgba[8];
    CHECK(!sr_bitmap_rgba(&b,raw,2,0,rgba,8));
    CHECK(!memcmp(rgba,"\xff\xff\xff\xff\0\0\0\xff",8));
    b.palette=1; CHECK(sr_bitmap_rgba(&b,raw,2,0,rgba,8));
    /* KRIG's original movement arrows use 16-bit RGB555. Compressed BITD
       rows store high and low bytes separately; raw rows store pixel pairs. */
    const uint8_t packed16[]={0x7c,0, 0x03,0xe0, 0,0x1f, 0xde,0xad,
        0xff,0xff, 0,0, 0x42,0x10, 0xbe,0xef};
    const uint8_t planar16[]={0x7c,0x03,0, 0,0xe0,0x1f, 0xde,0xad,
        0xff,0,0x42, 0xff,0,0x10, 0xbe,0xef};
    const uint8_t colors16[]={255,0,0,255, 0,255,0,255, 0,0,255,255,
        255,255,255,255, 0,0,0,255, 132,132,132,255};
    uint8_t pixels16[24];b=(SrBitmap){8,3,2,16,0,0,0,0,-1};
    CHECK(!sr_bitmap_rgba(&b,packed16,sizeof packed16,0,pixels16,sizeof pixels16));
    CHECK(!memcmp(pixels16,colors16,sizeof colors16));
    CHECK(!sr_bitmap_rgba(&b,planar16,sizeof planar16,1,pixels16,sizeof pixels16));
    CHECK(!memcmp(pixels16,colors16,sizeof colors16));
    CHECK(sr_bitmap_rgba(&b,packed16,15,0,pixels16,sizeof pixels16));
    CHECK(sr_bitmap_rgba(&b,packed16,16,0,pixels16,23));
    b.pitch=5;CHECK(sr_bitmap_rgba(&b,packed16,16,0,pixels16,24));
    uint8_t score[38]={0,0,0,38, 0,0,0,20, 0,0,0,0, 0,7,0,24,0,50,0,0,
                      0,10,0,4,0,0,0,1,0,18, 0,8,0,2,0,2,0,3};
    uint8_t state[SR_SCORE_BYTES]; uint32_t count;
    CHECK(!sr_score_frame(score,sizeof score,1,state,&count));
    CHECK(count==2 && state[1]==1 && state[3]==18);
    CHECK(!sr_score_frame(score,sizeof score,2,state,&count));
    CHECK(state[1]==1 && state[3]==3);
    CHECK(sr_score_frame(score,sizeof score,3,state,&count));
    score[24]=255; CHECK(sr_score_frame(score,sizeof score,1,state,&count));
    /* Original Year STXT metrics: size 18 Arial, dark red, 22px line. */
    uint8_t stxt[]={0,0,0,12, 0,0,0,4, 0,0,0,22, '1','5','4','7',
        0,1, 0,0,0,0, 0,22,0,17,0,1,0,0,0,18,0x33,0x33,0,0,0,0};
    SrText text; SrTextStyle style;
    CHECK(!sr_text_open(stxt,sizeof stxt,&text));
    CHECK(text.length==4 && text.style_count==1 && !memcmp(text.bytes,"1547",4));
    CHECK(!sr_text_style(&text,0,&style));
    CHECK(style.font_id==1 && style.size==18 && style.height==22 && style.ascent==17 && style.red==0x3333);
    CHECK(sr_text_style(&text,1,&style));
    for(size_t n=0;n<sizeof stxt;++n) CHECK(sr_text_open(stxt,n,&text));
    stxt[21]=5; CHECK(sr_text_open(stxt,sizeof stxt,&text)); stxt[21]=0;
    stxt[17]=2; CHECK(sr_text_open(stxt,sizeof stxt,&text));
    uint8_t metadata[28]={0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,23,0,58,0,23,0,0,0,23};
    SrCast cast={0}; cast.type=3; cast.specific=metadata; cast.specific_size=sizeof metadata;
    SrTextBox box;
    CHECK(!sr_text_box(&cast,&box) && box.right==58 && box.bottom==23 && box.text_height==23 && box.background[0]==65535);
    cast.specific_size=27; CHECK(sr_text_box(&cast,&box));
    uint8_t fmap[61]={0,0,0,36, 0,0,0,17};
    fmap[19]=1; fmap[41]=2; fmap[43]=1; fmap[47]=13;
    memcpy(fmap+48,"MS Sans Serif",13);
    SrFontMapping font;
    CHECK(!sr_fontmap_count(fmap,sizeof fmap,&count) && count==1);
    CHECK(!sr_fontmap_entry(fmap,sizeof fmap,0,&font));
    CHECK(font.platform==2 && font.id==1 && font.name_length==13 && !memcmp(font.name,"MS Sans Serif",13));
    CHECK(sr_fontmap_entry(fmap,sizeof fmap,1,&font));
    for(size_t n=0;n<sizeof fmap;++n) CHECK(sr_fontmap_entry(fmap,n,0,&font));
    fmap[39]=16; CHECK(sr_fontmap_entry(fmap,sizeof fmap,0,&font));
    /* Format-2 inline sound, first a standard mono unsigned-8 sample. */
    uint8_t snd[86]={0,2,0,0,0,1,0x80,0x51,0,0,0,0,0,14};
    snd[21]=3; snd[22]=0x56; snd[23]=0x22; snd[33]=3;
    snd[36]=0; snd[37]=128; snd[38]=255;
    SrSound sound;
    CHECK(!sr_sound_open(snd,39,&sound));
    CHECK(sound.frames==3 && sound.channels==1 && sound.bits==8 &&
          sound.rate_fixed==UINT32_C(0x56220000) && sound.samples==snd+36 && sound.loop_end==3);
    for(size_t n=0;n<39;++n) CHECK(sr_sound_open(snd,n,&sound));
    /* Extended header: stereo signed-16 BE, two interleaved frames. */
    memset(snd+34,0,sizeof snd-34);
    snd[21]=2; snd[33]=2; snd[34]=255; snd[39]=2; snd[63]=16;
    snd[78]=0x80; snd[80]=0x7f; snd[81]=0xff;
    CHECK(!sr_sound_open(snd,sizeof snd,&sound));
    CHECK(sound.frames==2 && sound.channels==2 && sound.bits==16 && sound.samples==snd+78);
    for(size_t n=0;n<sizeof snd;++n) CHECK(sr_sound_open(snd,n,&sound));
    snd[34]=254; CHECK(sr_sound_open(snd,sizeof snd,&sound)); snd[34]=255;
    snd[63]=24; CHECK(sr_sound_open(snd,sizeof snd,&sound)); snd[63]=16;
    snd[21]=3; CHECK(sr_sound_open(snd,sizeof snd,&sound)); snd[21]=2;
    snd[33]=3; CHECK(sr_sound_open(snd,sizeof snd,&sound)); snd[33]=2;
    snd[14]=1; CHECK(sr_sound_open(snd,sizeof snd,&sound)); snd[14]=0;
    snd[10]=255; CHECK(sr_sound_open(snd,sizeof snd,&sound));
    puts("BITD, palette, score deltas, text/fonts and bounded PCM sound parsing passed.");
    return 0;
}
