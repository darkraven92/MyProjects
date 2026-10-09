#include "assets.h"
#include <string.h>

static uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0]*256u+p[1]); }
static uint32_t read32(const uint8_t *p, int le) {
    return le ? (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24 :
        (uint32_t)p[3]|(uint32_t)p[2]<<8|(uint32_t)p[1]<<16|(uint32_t)p[0]<<24;
}
static int fits(size_t size, size_t pos, size_t n) { return pos<=size && n<=size-pos; }
const char *sr_sound_open(const uint8_t *d,size_t size,SrSound *s) {
    if(!d || !s || size<14) return "truncated sound resource";
    if(be16(d)!=2 || be16(d+4)!=1 || be16(d+6)!=0x8051)
        return "unsupported sound command list";
    size_t offset=read32(d+10,0);
    if(offset<14 || !fits(size,offset,22)) return "truncated sound header";
    const uint8_t *h=d+offset;
    if(read32(h,0)) return "external sound samples unsupported";
    SrSound out={0}; out.channels=1; out.bits=8;
    out.frames=read32(h+4,0); out.rate_fixed=read32(h+8,0);
    out.loop_start=read32(h+12,0); out.loop_end=read32(h+16,0);
    size_t header=22;
    if(h[20]==255) {
        header=64;
        if(!fits(size,offset,header)) return "truncated extended sound header";
        uint32_t channels=read32(h+4,0);
        if(channels!=1 && channels!=2) return "unsupported sound channels";
        out.channels=(uint16_t)channels; out.bits=be16(h+48);
        out.frames=read32(h+22,0);
    } else if(h[20]!=0) return "compressed sound unsupported";
    if((out.bits!=8 && out.bits!=16) || !out.rate_fixed || !out.frames)
        return "invalid PCM sound format";
    uint64_t bytes=(uint64_t)out.frames*out.channels*(out.bits/8);
    if(bytes>size-offset-header) return "truncated sound samples";
    if(out.loop_start>out.loop_end || out.loop_end>out.frames) return "invalid sound loop";
    out.samples=h+header; *s=out;
    return NULL;
}
const char *sr_find(const SrArchive *a, const char tag[4], SrResource *r) {
    for (uint32_t i=0;i<a->count;++i) {
        const char *e=sr_archive_resource(a,i,r);
        if (e) return e;
        if (r->active && !memcmp(r->tag,tag,4)) return NULL;
    }
    return "resource not found";
}
const char *sr_cast_range(const SrArchive *a, uint16_t *first, uint32_t *count) {
    SrResource config, cast;
    const char *e=sr_find(a,"VWCF",&config);
    if (e) return e;
    if (config.size<16) return "truncated VWCF";
    *first=be16(a->data+config.offset+8+12);
    e=sr_find(a,"CAS*",&cast);
    if (e) return e;
    if (cast.size%4) return "invalid CAS* size";
    *count=cast.size/4;
    if (*count>65536u-*first) return "cast range overflow";
    return NULL;
}
const char *sr_cast(const SrArchive *a, uint16_t number, SrCast *cast) {
    uint16_t first; uint32_t count; SrResource table,r;
    const char *e=sr_cast_range(a,&first,&count);
    if (e) return e;
    if (number<first || (uint32_t)(number-first)>=count) return "cast number outside range";
    e=sr_find(a,"CAS*",&table); if(e) return e;
    uint32_t id=read32(a->data+table.offset+8+(number-first)*4,0);
    memset(cast,0,sizeof *cast); cast->number=number;
    if (!id) return NULL; /* Empty slot. */
    e=sr_archive_resource(a,id,&r); if(e) return e;
    if (!r.active || memcmp(r.tag,"CASt",4) || r.size<12) return "invalid CASt reference";
    const uint8_t *p=a->data+r.offset+8;
    uint32_t info=read32(p+4,0), specific=read32(p+8,0);
    if (!fits(r.size,12,info) || !fits(r.size,12+(size_t)info,specific)) return "truncated CASt";
    cast->resource=id; cast->type=read32(p,0);
    cast->specific=p+12+info; cast->specific_size=specific;
    if (!info) return NULL;
    p+=12;
    if (info<20) return "truncated cast info";
    size_t table_pos=read32(p,0);
    if (!fits(info,table_pos,2)) return "invalid cast info table";
    uint16_t n=be16(p+table_pos);
    size_t table_size=2+(size_t)n*4+4;
    if (!fits(info,table_pos,table_size)) return "truncated cast info offsets";
    size_t base=table_pos+table_size, bytes=read32(p+base-4,0);
    if (!fits(info,base,bytes)) return "cast info data outside payload";
    if (n<2) return NULL;
    size_t start=read32(p+table_pos+6,0);
    size_t end=n>2 ? read32(p+table_pos+10,0) : bytes;
    if(start>end || end>bytes) return "invalid cast name range";
    if(start==end) return NULL;
    uint8_t len=p[base+start];
    if ((size_t)len+1>end-start) return "truncated cast name";
    memcpy(cast->name,p+base+start+1,len);
    return NULL;
}
const char *sr_child(const SrArchive *a, uint32_t parent, const char tag[4], SrResource *r) {
    SrResource keys;
    const char *e=sr_find(a,"KEY*",&keys); if(e) return e;
    const uint8_t *p=a->data+keys.offset+8;
    if(keys.size<12) return "truncated KEY*";
    int le=a->little_endian;
    uint16_t stride=le ? (uint16_t)(p[0]+p[1]*256u) : be16(p);
    uint32_t count=read32(p+4,le);
    if(stride!=12 || count>(keys.size-12)/12) return "invalid KEY* dimensions";
    for(uint32_t i=0;i<count;++i) {
        const uint8_t *k=p+12+(size_t)i*12;
        if(read32(k+4,le)!=parent) continue;
        char t[4]; for(int j=0;j<4;++j) t[j]=(char)k[8+(le?3-j:j)];
        if(memcmp(t,tag,4)) continue;
        e=sr_archive_resource(a,read32(k,le),r); if(e) return e;
        if(!r->active || memcmp(r->tag,tag,4)) return "invalid KEY* child";
        return NULL;
    }
    return "child resource not found";
}
const char *sr_bitmap_info(const SrCast *c, SrBitmap *b) {
    if(c->type!=1 || c->specific_size<28) return "unsupported bitmap metadata";
    const uint8_t *p=c->specific;
    b->pitch=be16(p)&0xfff;
    b->top=(int16_t)be16(p+2); b->left=(int16_t)be16(p+4);
    int height=(int16_t)be16(p+6)-b->top, width=(int16_t)be16(p+8)-b->left;
    if(width<=0 || height<=0 || width>4096 || height>4096) return "invalid bitmap dimensions";
    b->width=(uint16_t)width; b->height=(uint16_t)height;
    b->reg_y=(int16_t)be16(p+18); b->reg_x=(int16_t)be16(p+20);
    b->depth=p[23] ? p[23] : 1; b->palette=(int16_t)be16(p+26);
    if(b->depth!=8 && b->depth!=16 && b->depth!=32) return "unsupported bitmap depth (only 8/16/32)";
    if(b->pitch<(uint32_t)width*b->depth/8) return "bitmap pitch too short";
    return NULL;
}
const char *sr_text_open(const uint8_t *data,size_t size,SrText *text) {
    if(!data || !text || size<12) return "truncated STXT header";
    size_t offset=read32(data,0),length=read32(data+4,0),formats=read32(data+8,0);
    if(offset!=12 || !fits(size,offset,length) || !fits(size,offset+length,formats) || formats<2)
        return "invalid STXT ranges";
    const uint8_t *style=data+offset+length;
    uint16_t count=be16(style);
    if((size_t)count>(formats-2)/20) return "truncated STXT styles";
    uint32_t previous=0;
    for(uint16_t i=0;i<count;++i) {
        uint32_t start=read32(style+2+(size_t)i*20,0);
        if(start>length || (i && start<previous)) return "invalid STXT style range";
        previous=start;
    }
    *text=(SrText){data+offset,style+2,(uint32_t)length,count};
    return NULL;
}
const char *sr_text_style(const SrText *text,uint16_t index,SrTextStyle *style) {
    if(!text || !style || index>=text->style_count) return "invalid STXT style index";
    const uint8_t *p=text->styles+(size_t)index*20;
    *style=(SrTextStyle){read32(p,0),be16(p+4),be16(p+6),be16(p+8),
        be16(p+12),be16(p+14),be16(p+16),be16(p+18),p[10]};
    return NULL;
}
const char *sr_text_box(const SrCast *cast,SrTextBox *box) {
    if(!cast || !box || cast->type!=3 || cast->specific_size<28) return "unsupported text box metadata";
    const uint8_t *p=cast->specific;
    *box=(SrTextBox){p[0],p[1],p[2],p[3],p[24],p[25],(int16_t)be16(p+4),
        (int16_t)be16(p+12),(int16_t)be16(p+14),(int16_t)be16(p+16),
        (int16_t)be16(p+18),(int16_t)be16(p+20),
        {be16(p+6),be16(p+8),be16(p+10)},be16(p+22),be16(p+26)};
    if(box->bottom<box->top || box->right<box->left) return "invalid text box rectangle";
    return NULL;
}
const char *sr_fontmap_count(const uint8_t *data,size_t size,uint32_t *count) {
    if(!data || !count || size<36) return "truncated Fmap header";
    uint32_t map_size=read32(data,0),names_size=read32(data+4,0),n=read32(data+16,0);
    if(map_size<28 || !fits(size,8,map_size) || !fits(size,8+(size_t)map_size,names_size) || n>(map_size-28)/8)
        return "invalid Fmap ranges";
    *count=n;
    return NULL;
}
const char *sr_fontmap_entry(const uint8_t *data,size_t size,uint32_t index,SrFontMapping *font) {
    uint32_t count;
    const char *error=sr_fontmap_count(data,size,&count);
    if(error) return error;
    if(!font || index>=count) return "invalid Fmap index";
    const uint8_t *p=data+36+(size_t)index*8;
    size_t names_size=read32(data+4,0),offset=read32(p,0);
    const uint8_t *names=data+8+(size_t)read32(data,0);
    if(!fits(names_size,offset,4)) return "invalid Fmap name offset";
    uint32_t length=read32(names+offset,0);
    if(!fits(names_size,offset+4,length)) return "truncated Fmap name";
    *font=(SrFontMapping){be16(p+4),be16(p+6),names+offset+4,length};
    return NULL;
}
const char *sr_bitd_unpack(const uint8_t *src, size_t size, uint8_t *dst, size_t expected) {
    if(!src || !dst || !expected) return "invalid BITD buffer";
    if(size==expected) { memcpy(dst,src,size); return NULL; }
    size_t i=0,o=0;
    while(i<size) {
        uint8_t op=src[i++]; size_t n=op<128 ? (size_t)op+1 : 257u-op;
        if(n>expected-o) return "BITD expands beyond image";
        if(op<128) {
            if(n>size-i) return "truncated BITD literal";
            memcpy(dst+o,src+i,n); i+=n;
        } else {
            if(i==size) return "truncated BITD repeat";
            memset(dst+o,src[i++],n);
        }
        o+=n;
    }
    return o==expected ? NULL : "BITD ends before image";
}
void sr_mac_palette(uint8_t rgb[768]) {
    unsigned i=0;
    for(int r=255;r>=0;r-=51) for(int g=255;g>=0;g-=51) for(int b=255;b>=0;b-=51) {
        if(!r && !g && !b) continue;
        rgb[i++]=(uint8_t)r; rgb[i++]=(uint8_t)g; rgb[i++]=(uint8_t)b;
    }
    const uint8_t ramp[]={238,221,187,170,136,119,85,68,34,17};
    for(int channel=0;channel<4;++channel) for(unsigned n=0;n<10;++n)
        for(int c=0;c<3;++c) rgb[i++]=(channel==c || channel==3)?ramp[n]:0;
    rgb[765]=rgb[766]=rgb[767]=0;
}
const char *sr_bitmap_rgba(const SrBitmap *b, const uint8_t *raw, size_t size,
                          int compressed, uint8_t *out, size_t capacity) {
    if(!b || !raw || !out || !b->width || !b->height ||
       b->pitch<(size_t)b->width*b->depth/8 || size<(size_t)b->pitch*b->height ||
       capacity<(size_t)b->width*b->height*4) return "short pixel buffer";
    if(b->depth!=8 && b->depth!=16 && b->depth!=32) return "unsupported pixel depth";
    if(b->depth==8 && b->palette!=0) return "unsupported palette (only system Mac)";
    uint8_t palette[768]; sr_mac_palette(palette);
    for(unsigned y=0;y<b->height;++y) for(unsigned x=0;x<b->width;++x) {
        uint8_t *q=out+((size_t)y*b->width+x)*4;
        const uint8_t *row=raw+(size_t)y*b->pitch;
        if(b->depth==8) memcpy(q,palette+row[x]*3,3);
        else if(b->depth==16) {
            /* Director RGB555: big-endian pairs when raw, high/low byte
               planes within each row after BITD decompression. */
            unsigned value=compressed?((unsigned)row[x]<<8)|row[b->width+x]:be16(row+x*2);
            for(unsigned c=0;c<3;++c) {
                unsigned component=(value>>(10-c*5))&31;
                q[c]=(uint8_t)((component<<3)|(component>>2));
            }
        }
        else for(unsigned c=0;c<3;++c) q[c]=row[compressed?(c+1)*b->width+x:x*4+c+1];
        q[3]=255;
    }
    return NULL;
}
const char *sr_score_frame(const uint8_t *d, size_t size, uint32_t frame,
                          uint8_t state[SR_SCORE_BYTES], uint32_t *count) {
    if(!d || !state || !count || size<20) return "truncated score";
    if(read32(d,0)!=size || read32(d+4,0)!=20 || be16(d+12)!=7 || be16(d+14)!=24)
        return "unsupported score layout";
    uint8_t current[SR_SCORE_BYTES]={0}; memset(state,0,SR_SCORE_BYTES);
    size_t pos=20; *count=0;
    while(pos<size) {
        if(!fits(size,pos,2)) return "truncated frame size";
        size_t n=be16(d+pos);
        if(!n) break;
        if(n<2 || !fits(size,pos,n)) return "invalid score frame size";
        size_t end=pos+n; pos+=2;
        while(pos<end) {
            if(!fits(end,pos,4)) return "truncated score delta header";
            size_t length=be16(d+pos), offset=be16(d+pos+2); pos+=4;
            if(!fits(end,pos,length) || !fits(SR_SCORE_BYTES,offset,length)) return "score delta outside bounds";
            memcpy(current+offset,d+pos,length); pos+=length;
        }
        ++*count;
        if(*count==frame) memcpy(state,current,SR_SCORE_BYTES);
    }
    if(frame>*count) return "frame outside score";
    return NULL;
}
