#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint8_t a,f,b,c,d,e,h,l;
    uint16_t sp,pc,div,rom0,romN;
    uint8_t ime,halted,stopped,halt_bug,cgb,compat,speed,boot,ram_enabled,ram_bank;
    uint8_t line,x,serial_bits,ie,cart_ir,rumble;
    uint8_t io[128],rtc[5],latched_rtc[5];
    uint32_t clock_rate;
} GBSView;
typedef struct {
    uint8_t fetcher,mode,bg_size,obj_size,bg_read,obj_read,window,selected_objects;
    int16_t x,window_y;
    uint8_t tile,attributes,bg_pixels[16],obj_pixels[16],obj_priority[16];
    uint8_t active[4],sample[4],volume[4];
    uint16_t length[4],period[4],countdown[4];
} GBSInspect;
typedef struct {
    uint32_t rate;
    uint8_t buttons[4],mute[4],gain[4],palette;
    int16_t tilt_x,tilt_y;
    uint8_t ppu_watch;
} GBSSettings;
typedef struct {
    uint8_t (*read)(void*,uint16_t,uint8_t,uint8_t);
    uint8_t (*write)(void*,uint16_t,uint8_t,uint8_t);
    void (*sample)(void*,int16_t,int16_t,const uint8_t*);
    void (*frame)(void*);
    uint8_t (*serial)(void*,uint8_t);
    void (*serial_byte)(void*,uint8_t);
    uint8_t (*infrared)(void*);
    void (*rumble)(void*,double);
    void (*camera_update)(void*,uint8_t*);
    void (*print)(void*,const uint32_t*,unsigned,unsigned,unsigned);
    void (*ppu_read)(void*,uint16_t,uint8_t,uint8_t);
} GBSCallbacks;
typedef struct GBSApi {
    uint32_t version,struct_size;
    void* (*create)(const GBSCallbacks*,void*);
    void (*destroy)(void*);
    int (*reset)(void*,unsigned,const uint8_t*,size_t,const uint8_t*,size_t,const uint8_t*,size_t);
    unsigned (*step)(void*);
    void (*view)(void*,GBSView*);
    void* (*memory)(void*,unsigned,size_t*);
    uint8_t (*read)(void*,uint16_t);
    void (*write)(void*,uint16_t,uint8_t);
    void (*settings)(void*,const GBSSettings*);
    size_t (*state_size)(void*);
    void (*save_state)(void*,uint8_t*);
    int (*load_state)(void*,const uint8_t*,size_t);
    void (*serial_pulse)(void*,uint8_t);
    uint8_t (*serial_out)(void*);
    size_t (*battery_size)(void*);
    int (*save_battery)(void*,uint8_t*,size_t);
    void (*load_battery)(void*,const uint8_t*,size_t);
    unsigned (*screen_size)(void*,unsigned*);
    const uint32_t* (*pixels)(void*);
    void (*printer)(void*,uint8_t);
    void (*set_registers)(void*,const GBSView*);
    void (*refresh_palettes)(void*);
    void (*test_ram)(void*);
    void (*inspect)(void*,GBSInspect*);
} GBSApi;
#if defined(_WIN32) && defined(GB_INTERNAL)
__declspec(dllexport)
#endif
const GBSApi* gbs_api_v1(void);
#ifdef __cplusplus
}
#endif
