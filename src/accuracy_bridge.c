/* Narrow C ABI between Studio and the licensed SameBoy hardware engine. */
#define GB_INTERNAL
#include "accuracy_api.h"
#include "mbc6.h"
#include "../third_party/sameboy/Core/gb.h"
#include "../third_party/sameboy/boot_data.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    GB_gameboy_t gb;
    GBSMBC6 *mbc6;
    GBSCallbacks callbacks;
    void *user;
    uint32_t pixels[256*224];
    uint8_t camera[128*112];
    GBSSettings settings;
    GB_palette_t palette;
    uint8_t running,booting,outgoing,sent_byte,sent_bits;
} Engine;
static Engine *engine(GB_gameboy_t *gb){return GB_get_user_data(gb);}
static uint32_t rgb(GB_gameboy_t *gb,uint8_t r,uint8_t g,uint8_t b){(void)gb;return 0xFF000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b;}
static void log_message(GB_gameboy_t *gb,const char *message,GB_log_attributes_t attr){(void)gb;(void)message;(void)attr;}
static uint8_t read_bus(GB_gameboy_t *gb,uint16_t addr,uint8_t data){
    Engine *e=engine(gb);
    if(e->mbc6&&(addr<0x8000||(addr>=0xA000&&addr<0xC000)))data=mbc6_read(e->mbc6,gb->rom,gb->rom_size,addr);
    if(e->running&&!e->booting&&e->callbacks.read){
        uint8_t blocked=(addr>=0x8000&&addr<0xA000&&gb->vram_read_blocked)||(addr>=0xFE00&&addr<0xFEA0&&gb->oam_read_blocked);
        return e->callbacks.read(e->user,addr,data,blocked);
    }return data;
}
static bool write_bus(GB_gameboy_t *gb,uint16_t addr,uint8_t data){
    Engine *e=engine(gb);
    if(e->running&&!e->booting&&e->callbacks.write){
        uint8_t blocked=(addr>=0x8000&&addr<0xA000&&gb->vram_write_blocked)||(addr>=0xFE00&&addr<0xFEA0&&gb->oam_write_blocked);
        if(!e->callbacks.write(e->user,addr,data,blocked))return false;
    }if(e->mbc6&&(addr<0x8000||(addr>=0xA000&&addr<0xC000))){mbc6_write(e->mbc6,addr,data);return false;}return true;
}
static void frame(GB_gameboy_t *gb,GB_vblank_type_t type){Engine *e=engine(gb);if(!e->booting&&type!=GB_VBLANK_TYPE_REPEAT&&e->callbacks.frame)e->callbacks.frame(e->user);}
static void sample(GB_gameboy_t *gb,GB_sample_t *s){Engine *e=engine(gb);if(e->booting||!e->callbacks.sample)return;
    uint8_t channels[4];for(unsigned i=0;i<4;++i)channels[i]=GB_get_channel_amplitude(gb,(GB_channel_t)i);
    e->callbacks.sample(e->user,s->left,s->right,channels);
}
static void serial_start(GB_gameboy_t *gb,bool bit){Engine *e=engine(gb);e->outgoing=bit;
    if(!gb->serial_count){e->sent_bits=0;e->sent_byte=gb->io_registers[GB_IO_SB];}}
static bool serial_end(GB_gameboy_t *gb){Engine *e=engine(gb);uint8_t bit=e->callbacks.serial?e->callbacks.serial(e->user,e->outgoing):1;
    if(++e->sent_bits==8){e->sent_bits=0;if(!e->booting&&e->callbacks.serial_byte)e->callbacks.serial_byte(e->user,e->sent_byte);}return bit!=0;}
static void rumble(GB_gameboy_t *gb,double amplitude){Engine *e=engine(gb);if(e->callbacks.rumble)e->callbacks.rumble(e->user,amplitude);}
static uint8_t camera_pixel(GB_gameboy_t *gb,uint8_t x,uint8_t y){Engine *e=engine(gb);return x<128&&y<112?e->camera[y*128+x]:0;}
static void camera_update(GB_gameboy_t *gb){Engine *e=engine(gb);if(e->callbacks.camera_update)e->callbacks.camera_update(e->user,e->camera);GB_camera_updated(gb);}
static void print_image(GB_gameboy_t *gb,uint32_t *p,uint8_t height,uint8_t top,uint8_t bottom,uint8_t exposure){Engine *e=engine(gb);(void)exposure;if(e->callbacks.print)e->callbacks.print(e->user,p,height,(unsigned)top*8,(unsigned)bottom*8);}
static void ppu_read(GB_gameboy_t *g,uint16_t address,uint8_t value,bool blocked){Engine *e=engine(g);if(e->callbacks.ppu_read)e->callbacks.ppu_read(e->user,address,value,blocked);}
static void setup(Engine *e){GB_gameboy_t *g=&e->gb;GB_set_user_data(g,e);GB_set_pixels_output(g,e->pixels);
    GB_set_rgb_encode_callback(g,rgb);GB_set_color_correction_mode(g,GB_COLOR_CORRECTION_DISABLED);GB_set_border_mode(g,GB_BORDER_SGB);
    GB_set_log_callback(g,log_message);GB_set_vblank_callback(g,frame);GB_apu_set_sample_callback(g,sample);
    GB_set_read_memory_callback(g,read_bus);GB_set_write_memory_callback(g,write_bus);
    if(!g->printer_callback){GB_set_serial_transfer_bit_start_callback(g,serial_start);GB_set_serial_transfer_bit_end_callback(g,serial_end);}
    GB_set_rumble_callback(g,rumble);GB_set_camera_get_pixel_callback(g,camera_pixel);GB_set_camera_update_request_callback(g,camera_update);
    GB_set_rtc_mode(g,GB_RTC_MODE_ACCURATE);GB_set_turbo_mode(g,true,true);GB_set_turbo_cap(g,0);
    GB_set_emulate_joypad_bouncing(g,false);GB_set_allow_illegal_inputs(g,true);
    for(unsigned i=0;i<4;++i)g->apu_output.studio_channel_gain[i]=100;
}
static void *create(const GBSCallbacks *callbacks,void *user){
    Engine *e=calloc(1,sizeof(*e));if(!e)return NULL;e->callbacks=*callbacks;e->user=user;
    GB_random_set_enabled(false);GB_init(&e->gb,GB_MODEL_DMG_B);setup(e);return e;
}
static void destroy(void *ctx){Engine *e=ctx;if(!e)return;GB_free(&e->gb);free(e->mbc6);free(e);}
static const uint8_t *replacement(unsigned model,size_t *size){
    *size=256;switch(model){
    case GB_MODEL_MGB:return mgb_boot;case GB_MODEL_SGB:case GB_MODEL_SGB_PAL:return sgb_boot;
    case GB_MODEL_SGB2:return sgb2_boot;
    case GB_MODEL_CGB_0:*size=sizeof(cgb0_boot);return cgb0_boot;
    case GB_MODEL_CGB_A:case GB_MODEL_CGB_B:case GB_MODEL_CGB_C:case GB_MODEL_CGB_D:case GB_MODEL_CGB_E:*size=sizeof(cgb_boot);return cgb_boot;
    case GB_MODEL_AGB_A:case GB_MODEL_GBP_A:*size=sizeof(agb_boot);return agb_boot;
    default:return dmg_boot;}
}
static int reset(void *ctx,unsigned model,const uint8_t *rom,size_t romsize,const uint8_t *boot,size_t bootsize,const uint8_t *battery,size_t batsize){
    Engine *e=ctx;if(!rom||romsize<0x150)return 0;
    if(rom[0x147]==0x20){if(!e->mbc6)e->mbc6=malloc(sizeof(*e->mbc6));if(!e->mbc6)return 0;mbc6_reset(e->mbc6);}else {free(e->mbc6);e->mbc6=NULL;}
    GB_switch_model_and_reset(&e->gb,(GB_model_t)model);setup(e);GB_load_rom_from_buffer(&e->gb,rom,romsize);GB_reset(&e->gb);setup(e);
    e->sent_byte=e->sent_bits=0;memset(&e->settings,0,sizeof(e->settings));e->settings.palette=255;memset(e->pixels,0xFF,sizeof(e->pixels));
    if(boot&&bootsize)GB_load_boot_rom_from_buffer(&e->gb,boot,bootsize);
    else {boot=replacement(model,&bootsize);GB_load_boot_rom_from_buffer(&e->gb,boot,bootsize);e->booting=1;
        uint64_t cycles=0;while(!e->gb.boot_rom_finished&&cycles<4194304u*16u)cycles+=GB_run(&e->gb);
        e->booting=0;if(!e->gb.boot_rom_finished)return 0;
    }
    if(e->mbc6&&battery&&batsize==8+32768+1048576+256&&!memcmp(battery,"M6BT1",5)){
        e->mbc6->protect_sector0=battery[5]&1;memcpy(e->mbc6->ram,battery+8,32768);memcpy(e->mbc6->flash,battery+8+32768,1048576);memcpy(e->mbc6->hidden_data,battery+8+32768+1048576,256);
    }else if(battery&&batsize){if(e->mbc6&&batsize==32768)memcpy(e->mbc6->ram,battery,32768);else GB_load_battery_from_buffer(&e->gb,battery,batsize);}return 1;
}
static unsigned step(void *ctx){Engine *e=ctx;if(e->callbacks.infrared)GB_set_infrared_input(&e->gb,e->callbacks.infrared(e->user)!=0);
    e->running=1;unsigned cycles=GB_run(&e->gb);e->running=0;return cycles/2;
}
static void view(void *ctx,GBSView *v){Engine *e=ctx;GB_gameboy_t *g=&e->gb;memset(v,0,sizeof(*v));
    v->a=g->a;v->f=g->f;v->b=g->b;v->c=g->c;v->d=g->d;v->e=g->e;v->h=g->h;v->l=g->l;v->sp=g->sp;v->pc=g->pc;
    v->div=g->div_counter;v->rom0=g->mbc_rom0_bank;v->romN=g->mbc_rom_bank;v->ime=g->ime;v->halted=g->halted;v->stopped=g->stopped;v->halt_bug=g->halt_bug;
    v->cgb=GB_is_cgb(g);v->compat=v->cgb&&!g->cgb_mode;v->speed=g->cgb_double_speed;v->boot=!g->boot_rom_finished;
    if(e->mbc6){v->rom0=0;v->romN=e->mbc6->rom_bank[0]/2;}
    v->cart_ir=g->cart_ir;v->rumble=g->rumble_strength;
    v->ram_enabled=e->mbc6?e->mbc6->ram_enable:g->mbc_ram_enable;v->ram_bank=g->mbc_ram_bank;v->line=g->current_line;v->x=g->position_in_line;v->serial_bits=g->serial_count;v->ie=g->interrupt_enable;
    memcpy(v->io,g->io_registers,128);memcpy(v->rtc,g->rtc_real.data,5);memcpy(v->latched_rtc,g->rtc_latched.data,5);v->clock_rate=GB_get_clock_rate(g);
}
static void *memory(void *ctx,unsigned access,size_t *size){Engine *e=ctx;if(e->mbc6&&access==2){*size=32768;return e->mbc6->ram;}return GB_get_direct_access(&e->gb,(GB_direct_access_t)access,size,NULL);}
static uint8_t read_mem(void *ctx,uint16_t addr){return GB_safe_read_memory(&((Engine*)ctx)->gb,addr);}
static void write_mem(void *ctx,uint16_t addr,uint8_t value){GB_write_memory(&((Engine*)ctx)->gb,addr,value);}
static void settings(void *ctx,const GBSSettings *s){Engine *e=ctx;GB_gameboy_t *g=&e->gb;
    if(s->rate!=e->settings.rate){GB_apu_run(g,true);GB_set_sample_rate(g,s->rate);g->apu_output.sample_cycles=0;g->apu_output.sample_fraction=0;g->apu_output.cycles_since_render=0;}
    for(unsigned p=0;p<4;++p)if(s->buttons[p]!=e->settings.buttons[p])GB_set_key_mask_for_player(g,(GB_key_mask_t)s->buttons[p],p);
    for(unsigned i=0;i<4;++i){if(s->mute[i]!=e->settings.mute[i])GB_set_channel_muted(g,(GB_channel_t)i,s->mute[i]!=0);g->apu_output.studio_channel_gain[i]=s->gain[i];}
    if(s->tilt_x!=e->settings.tilt_x||s->tilt_y!=e->settings.tilt_y)GB_set_accelerometer_values(g,s->tilt_x/112.0,s->tilt_y/112.0);
    if(s->palette!=e->settings.palette){static const uint32_t palettes[4][4]={{0x9BBD0F,0x8BAC0F,0x306230,0x0F380F},{0xFFFFFF,0xAAAAAA,0x555555,0},{0xDCEAF6,0x8CA7C2,0x3E6991,0x192F4A},{0xFFE8B8,0xD6A078,0x89506B,0x34172A}};
        for(unsigned i=0;i<5;++i){uint32_t color=palettes[s->palette&3][i<4?3-i:0];e->palette.colors[i].r=color>>16;e->palette.colors[i].g=color>>8;e->palette.colors[i].b=color;}GB_set_palette(g,&e->palette);
    }g->studio_ppu_read_callback=s->ppu_watch?ppu_read:NULL;e->settings=*s;
}
enum { EXTRA_BASE=32+128*112, EXTRA_SIZE=EXTRA_BASE+256*224*4 };
static uint32_t le32(const uint8_t *b){return b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);}
static void put32(uint8_t *b,uint32_t n){for(unsigned i=0;i<4;++i)b[i]=n>>(i*8);}
static size_t state_size(void *ctx){Engine *e=ctx;GB_apu_run(&e->gb,true);GB_display_run(&e->gb,0,true);return GB_get_save_state_size(&e->gb)+8+EXTRA_SIZE+1+(e->mbc6?sizeof(*e->mbc6):0);}
static void save_state(void *ctx,uint8_t *b){Engine *e=ctx;GB_apu_run(&e->gb,true);GB_display_run(&e->gb,0,true);size_t n=GB_get_save_state_size(&e->gb);GB_save_state_to_buffer(&e->gb,b);
    size_t end=n-16,at=le32(b+n-8);for(unsigned i=0;i<128&&at+8<=n-8;++i){uint32_t size=le32(b+at+4);if(size>n-8-at-8)break;if(!memcmp(b+at,"END ",4)){end=at;break;}at+=8+size;}
    size_t extra_size=EXTRA_SIZE+1+(e->mbc6?sizeof(*e->mbc6):0);memmove(b+end+8+extra_size,b+end,n-end);uint8_t *data=b+end;memcpy(data,"GBSX",4);put32(data+4,extra_size);data+=8;
    for(unsigned p=0;p<4;++p){data[p]=0;for(unsigned k=0;k<8;++k)if(e->gb.keys[p][k])data[p]|=1u<<k;}
    data[4]=e->outgoing;data[5]=e->sent_byte;data[6]=e->sent_bits;data[7]=2;
    put32(data+8,e->settings.rate);data[12]=e->settings.palette;
    memcpy(data+13,e->settings.buttons,4);memcpy(data+17,e->settings.mute,4);memcpy(data+21,e->settings.gain,4);
    data[25]=(uint8_t)e->settings.tilt_x;data[26]=(uint8_t)(e->settings.tilt_x>>8);
    /* Native accelerometer values are reapplied from Studio's saved tilt on next step. */
    data[27]=(uint8_t)e->settings.tilt_y;data[28]=(uint8_t)(e->settings.tilt_y>>8);data[29]=data[30]=data[31]=0;memcpy(data+32,e->camera,sizeof(e->camera));for(unsigned i=0;i<256*224;++i)put32(data+EXTRA_BASE+i*4,e->pixels[i]);data[EXTRA_SIZE]=e->mbc6!=NULL;if(e->mbc6)memcpy(data+EXTRA_SIZE+1,e->mbc6,sizeof(*e->mbc6));
}
static int load_state(void *ctx,const uint8_t *b,size_t n){Engine *e=ctx;const uint8_t *extra=NULL;
    if(n>=16&&!memcmp(b+n-4,"BESS",4)){size_t at=le32(b+n-8);for(unsigned blocks=0;at+8<=n-8&&blocks<128;++blocks){uint32_t size=le32(b+at+4);at+=8;if(size>n-8-at)return 0;
        if(!memcmp(b+at-8,"GBSX",4)){if(size!=EXTRA_SIZE+1+(e->mbc6?sizeof(*e->mbc6):0)||extra)return 0;extra=b+at;}if(!memcmp(b+at-8,"END ",4))break;at+=size;}}
    if(extra){if(extra[4]>1||extra[6]>7||extra[7]!=2||(extra[12]>3&&extra[12]!=255))return 0;
        if(extra[EXTRA_SIZE]!=(e->mbc6!=NULL))return 0;
        if(e->mbc6){GBSMBC6 *check=malloc(sizeof(*check));if(!check)return 0;memcpy(check,extra+EXTRA_SIZE+1,sizeof(*check));
            int ok=check->rom_bank[0]<128&&check->rom_bank[1]<128&&check->ram_bank[0]<8&&check->ram_bank[1]<8&&check->program_count<=128&&check->program_base<1048576&&!(check->program_base&127)&&check->unlock<=5;
            free(check);if(!ok)return 0;}
        uint32_t rate=le32(extra+8);if(rate&&rate!=44100&&rate!=48000&&rate!=96000)return 0;
        for(unsigned i=0;i<4;++i)if(extra[17+i]>1||extra[21+i]>100)return 0;
    }
    int r=GB_load_state_from_buffer(&e->gb,b,n);if(r)return 0;
    if(extra){for(unsigned p=0;p<4;++p)for(unsigned k=0;k<8;++k)e->gb.keys[p][k]=(extra[p]>>k)&1;
        e->outgoing=extra[4];e->sent_byte=extra[5];e->sent_bits=extra[6];memcpy(e->camera,extra+32,sizeof(e->camera));for(unsigned i=0;i<256*224;++i)e->pixels[i]=le32(extra+EXTRA_BASE+i*4);if(e->mbc6)memcpy(e->mbc6,extra+EXTRA_SIZE+1,sizeof(*e->mbc6));}
    memset(&e->settings,0,sizeof(e->settings));e->settings.palette=255;setup(e);
    if(extra){GBSSettings saved={0};saved.rate=le32(extra+8);saved.palette=extra[12];memcpy(saved.buttons,extra+13,4);memcpy(saved.mute,extra+17,4);memcpy(saved.gain,extra+21,4);
        saved.tilt_x=(int16_t)(extra[25]|(extra[26]<<8));saved.tilt_y=(int16_t)(extra[27]|(extra[28]<<8));memcpy(e->settings.buttons,saved.buttons,4);
        if(saved.rate){settings(ctx,&saved);}else e->settings=saved;
    }return 1;
}
static void serial_pulse(void *ctx,uint8_t bit){GB_serial_set_data_bit(&((Engine*)ctx)->gb,bit!=0);}
static uint8_t serial_out(void *ctx){Engine *e=ctx;return (e->gb.io_registers[GB_IO_SC]&1)?e->outgoing:GB_serial_get_data_bit(&e->gb);}
static size_t battery_size(void *ctx){Engine *e=ctx;return e->mbc6?8+32768+1048576+256:(size_t)GB_save_battery_size(&e->gb);}
static int save_battery(void *ctx,uint8_t *b,size_t n){Engine *e=ctx;if(e->mbc6){if(n!=battery_size(ctx))return 0;memset(b,0,8);memcpy(b,"M6BT1",5);b[5]=e->mbc6->protect_sector0;memcpy(b+8,e->mbc6->ram,32768);memcpy(b+8+32768,e->mbc6->flash,1048576);memcpy(b+8+32768+1048576,e->mbc6->hidden_data,256);return 1;}return GB_save_battery_to_buffer(&e->gb,b,n)==0;}
static void load_battery(void *ctx,const uint8_t *b,size_t n){GB_load_battery_from_buffer(&((Engine*)ctx)->gb,b,n);}
static unsigned screen_size(void *ctx,unsigned *height){GB_gameboy_t *g=&((Engine*)ctx)->gb;*height=GB_get_screen_height(g);return GB_get_screen_width(g);}
static const uint32_t *pixels(void *ctx){return ((Engine*)ctx)->pixels;}
static void printer(void *ctx,uint8_t on){Engine *e=ctx;if(on)GB_connect_printer(&e->gb,print_image,NULL);else {GB_disconnect_serial(&e->gb);GB_set_serial_transfer_bit_start_callback(&e->gb,serial_start);GB_set_serial_transfer_bit_end_callback(&e->gb,serial_end);}}
static void set_registers(void *ctx,const GBSView *v){GB_gameboy_t *g=&((Engine*)ctx)->gb;g->a=v->a;g->f=v->f&0xF0;g->b=v->b;g->c=v->c;g->d=v->d;g->e=v->e;g->h=v->h;g->l=v->l;g->sp=v->sp;g->pc=v->pc;}
static void refresh_palettes(void *ctx){GB_gameboy_t *g=&((Engine*)ctx)->gb;for(unsigned i=0;i<64;i+=2){GB_palette_changed(g,true,i);GB_palette_changed(g,false,i);}}
static void test_ram(void *ctx){GB_gameboy_t *g=&((Engine*)ctx)->gb;if(g->cartridge_type->mbc_type!=GB_MBC1||g->mbc_ram_size)return;
    g->cartridge_type=&GB_cart_defs[2];g->mbc_ram_size=8192;g->mbc_ram=malloc(8192);if(!g->mbc_ram){g->mbc_ram_size=0;return;}memset(g->mbc_ram,0xFF,8192);}
static void inspect(void *ctx,GBSInspect *v){GB_gameboy_t *g=&((Engine*)ctx)->gb;memset(v,0,sizeof(*v));
    v->fetcher=g->fetcher_state;v->mode=g->io_registers[GB_IO_STAT]&3;v->bg_size=g->bg_fifo.size;v->obj_size=g->oam_fifo.size;v->bg_read=g->bg_fifo.read_end;v->obj_read=g->oam_fifo.read_end;
    v->window=g->wx_triggered;v->window_y=g->window_y;v->selected_objects=g->n_visible_objs;v->tile=g->current_tile;v->attributes=g->current_tile_attributes;v->x=g->position_in_line>240?(int16_t)g->position_in_line-256:g->position_in_line;
    for(unsigned i=0;i<16;++i){v->bg_pixels[i]=g->bg_fifo.fifo[(g->bg_fifo.read_end+i)&(GB_FIFO_LENGTH-1)].pixel;v->obj_pixels[i]=g->oam_fifo.fifo[(g->oam_fifo.read_end+i)&(GB_FIFO_LENGTH-1)].pixel;v->obj_priority[i]=g->oam_fifo.fifo[(g->oam_fifo.read_end+i)&(GB_FIFO_LENGTH-1)].priority;}
    for(unsigned i=0;i<4;++i){v->active[i]=g->apu.is_active[i];v->sample[i]=g->apu.samples[i];}
    for(unsigned i=0;i<2;++i){v->length[i]=g->apu.square_channels[i].pulse_length;v->volume[i]=g->apu.square_channels[i].current_volume;v->period[i]=g->apu.square_channels[i].sample_length;v->countdown[i]=g->apu.square_channels[i].sample_countdown;}
    v->length[2]=g->apu.wave_channel.pulse_length;v->period[2]=g->apu.wave_channel.sample_length;v->countdown[2]=g->apu.wave_channel.sample_countdown;v->volume[2]=(g->io_registers[GB_IO_NR32]>>5)&3;
    v->length[3]=g->apu.noise_channel.pulse_length;v->volume[3]=g->apu.noise_channel.current_volume;v->period[3]=g->io_registers[GB_IO_NR43];v->countdown[3]=g->apu.noise_channel.counter_countdown;
}
static const GBSApi api={3,sizeof(GBSApi),create,destroy,reset,step,view,memory,read_mem,write_mem,settings,state_size,save_state,load_state,serial_pulse,serial_out,battery_size,save_battery,load_battery,screen_size,pixels,printer,set_registers,refresh_palettes,test_ram,inspect};
const GBSApi *gbs_api_v1(void){return &api;}
