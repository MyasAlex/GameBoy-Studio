#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gb.h"

/* Place a valid OBJ fetch immediately before/after the low-address latch.
   The golden Mealybug ROM tests cover full-frame hardware output; this test
   verifies that saving in either one-dot continuation preserves that latch. */
static void quiet(GB_gameboy_t *gb, const char *text, GB_log_attributes_t flags) {(void)gb;(void)text;(void)flags;}
static int setup(GB_gameboy_t *g, GB_model_t model, uint32_t *screen) {
    unsigned char rom[32768] = {0}; rom[0x143] = 0x80;
    if (!GB_init(g, model)) return 0;
    GB_set_log_callback(g, quiet);
    GB_load_rom_from_buffer(g, rom, sizeof rom);
    GB_set_pixels_output(g, screen);
    g->boot_rom_finished=true; g->pc=0x100; g->cgb_mode=GB_is_cgb(g);
    g->io_registers[GB_IO_LCDC]=0x87; g->io_registers[GB_IO_STAT]=3;
    g->current_line=68; g->io_registers[GB_IO_LY]=68;
    g->display_state=44; g->display_cycles=0;
    g->cycles_for_line=136; g->position_in_line=24; g->lcd_x=24;
    g->n_visible_objs=1; g->visible_objs[0]=0;
    g->objects_x[0]=32; g->objects_y[0]=80;
    g->mode2_y_bus=0x4c; g->object_flags=0x40; g->during_object_fetch=true;
    g->object_low_line_address=0x120; g->object_fetch_aborted=false;
    g->fetcher_state=6; g->bg_fifo.size=8; g->bg_fifo.read_end=0;
    g->vram[0x4d6]=0xa5; g->vram[0x4d7]=0xc3;
    g->vram[0x4c6]=0x5a; g->vram[0x4c7]=0x3c;
    return 1;
}
static int equal_saved(GB_gameboy_t *a, GB_gameboy_t *b) {
    size_t size=GB_get_save_state_size(a);
    unsigned char *aa=malloc(size), *bb=malloc(size);
    GB_save_state_to_buffer(a,aa); GB_save_state_to_buffer(b,bb);
    int equal=GB_get_save_state_size(b)==size && !memcmp(aa,bb,size);
    free(aa); free(bb); return equal;
}
static int check_model(GB_model_t model, int restore_at) {
    GB_gameboy_t *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b));
    uint32_t *screen_a=calloc(160*144,4),*screen_b=calloc(160*144,4);
    if (!a || !b || !screen_a || !screen_b) return 0;
    if (!setup(a,model,screen_a) || !setup(b,model,screen_b)) return 0;
    if (restore_at==20) {
        GB_display_run(a,2,true);
        if (a->display_state != 20) return 0;
        a->io_registers[GB_IO_LCDC]=0x83;
    }
    size_t size=GB_get_save_state_size(a); unsigned char *snapshot=malloc(size);
    GB_save_state_to_buffer(a,snapshot);
    int loaded=GB_load_state_from_buffer(b,snapshot,size)==0;
    free(snapshot);
    if (!loaded || b->display_state!=restore_at || b->object_low_line_address!=a->object_low_line_address) {printf("LOAD FAIL model=%x at=%d loaded=%d state=%d addr=%x vs %x\n",model,restore_at,loaded,b->display_state,b->object_low_line_address,a->object_low_line_address);return 0;}
    if (restore_at==44) {
        GB_display_run(a,2,true); GB_display_run(b,2,true);
        if (a->display_state!=20 || b->display_state!=20) {printf("STEP FAIL model=%x at=%d state=%d/%d\n",model,restore_at,a->display_state,b->display_state); return 0;}
        a->io_registers[GB_IO_LCDC]=b->io_registers[GB_IO_LCDC]=0x83;
    }
    GB_display_run(a,2,true); GB_display_run(b,2,true);
    unsigned expected=GB_is_cgb(a)?0xa5:0x5a;
    int okay=a->display_state==39 && b->display_state==39 &&
             a->object_tile_data[0]==expected && b->object_tile_data[0]==expected;
    GB_display_run(a,4,true); GB_display_run(b,4,true);
    GB_display_run(a,2,true); GB_display_run(b,2,true);
    okay &= a->object_tile_data[1]==0x3c && b->object_tile_data[1]==0x3c;
    okay &= equal_saved(a,b) && !memcmp(screen_a,screen_b,160*144*4);
    printf("%s model=%x restored_at=%d low=%02x high=%02x native_continuation_equal=%s\n",
        okay?"PASS":"FAIL",model,restore_at,a->object_tile_data[0],a->object_tile_data[1],okay?"yes":"no");
    GB_free(a); GB_free(b); free(a); free(b); free(screen_a); free(screen_b);
    return okay;
}
static int check_tile_bus_checkpoint(GB_model_t model) {
    GB_gameboy_t *a=calloc(1,sizeof(*a)), *b=calloc(1,sizeof(*b));
    uint32_t *sa=calloc(160*144,4), *sb=calloc(160*144,4);
    if (!a || !b || !sa || !sb || !setup(a,model,sa) || !setup(b,model,sb)) return 0;
    /* A rising TILE_SEL edge substitutes 0xaa at the pixel latch while
       the physical VRAM read returns 0x33. Save after that read, while
       both distinct values are live and the FIFO prevents a new push. */
    a->display_state=21; a->display_cycles=0; a->fetcher_state=5;
    a->n_visible_objs=0; a->during_object_fetch=false;
    a->last_tileset=false; a->tile_sel_glitch=true;
    a->last_tile_data_address=0x1001; a->vram[0x1001]=0x33;
    a->data_for_sel_glitch=0xaa; a->current_tile_data[0]=0x55;
    GB_display_run(a,2,true);
    int okay=a->display_state==21 && a->last_tile_data_bus==0x33 &&
             a->current_tile_data[1]==0xaa;
    size_t size=GB_get_save_state_size(a); unsigned char *snapshot=malloc(size);
    if (!snapshot) return 0;
    GB_save_state_to_buffer(a,snapshot);
    b->last_tile_data_bus=0x99; b->current_tile_data[1]=0x77;
    int loaded=GB_load_state_from_buffer(b,snapshot,size)==0;
    free(snapshot);
    okay &= loaded && b->last_tile_data_bus==0x33 && b->current_tile_data[1]==0xaa;
    a->tile_sel_glitch=b->tile_sel_glitch=false;
    GB_display_run(a,64,true); GB_display_run(b,64,true);
    okay &= equal_saved(a,b) && !memcmp(sa,sb,160*144*4);
    printf("%s model=%x glitched_high_checkpoint raw=33 output=aa continuation_equal=%s\n",
           okay?"PASS":"FAIL",model,okay?"yes":"no");
    GB_free(a); GB_free(b); free(a);free(b);free(sa);free(sb);
    return okay;
}
static void observe_ppu_read(GB_gameboy_t *g,uint16_t address,uint8_t value,bool object) {
    (void)g;(void)address;(void)value;(void)object;
}
static int check_batched_tile_bus(GB_model_t model) {
    int okay=1;
    for (unsigned flip=0;flip<2;flip++) for (unsigned scx=0;scx<8;scx++) {
        GB_gameboy_t *g[2]={calloc(1,sizeof(*g[0])),calloc(1,sizeof(*g[1]))};
        uint32_t *screen[2]={calloc(160*144,4),calloc(160*144,4)};
        for (unsigned slow=0;slow<2;slow++) {
            if (!g[slow] || !screen[slow] || !setup(g[slow],model,screen[slow])) return 0;
            GB_gameboy_t *p=g[slow];
            p->display_state=32; p->display_cycles=0;
            p->current_line=16; p->io_registers[GB_IO_LY]=16;
            p->io_registers[GB_IO_LCDC]=0x93; p->io_registers[GB_IO_SCX]=scx;
            p->position_in_line=(uint8_t)-16; p->cycles_for_line=89;
            p->n_visible_objs=0; p->during_object_fetch=false;
            /* Observing VRAM reads forces the dot-by-dot PPU path. */
            p->studio_ppu_read_callback=slow?observe_ppu_read:NULL;
            for (unsigned color=0;color<4;color++) p->background_palettes_rgb[color]=0x555555*color;
            for (unsigned x=0;x<32;x++) {
                p->vram[0x1840+x]=x; p->vram[0x3840+x]=flip?0x20:0;
                for (unsigned y=0;y<8;y++) {
                    p->vram[x*16+y*2]=0x80+x;
                    p->vram[x*16+y*2+1]=0x40+x;
                }
            }
            GB_display_run(p,400,true);
        }
        /* Horizontal flip changes screen order, never the physical bus byte.
           Fractional SCX determines which final tile read reaches that bus. */
        unsigned expected=scx<6?0x54:0x55;
        int same=g[0]->last_tile_data_bus==expected && g[1]->last_tile_data_bus==expected &&
                 !memcmp(screen[0],screen[1],160*144*4);
        if (!same) printf("FAIL model=%x SCX=%u flip=%u fast_bus=%02x stepped_bus=%02x expected=%02x\n",
                         model,scx,flip,g[0]->last_tile_data_bus,g[1]->last_tile_data_bus,expected);
        okay &= same;
        for (unsigned i=0;i<2;i++) {GB_free(g[i]);free(g[i]);free(screen[i]);}
    }
    printf("%s model=%x batched_vs_stepped_bus_and_pixels SCX=0..7 horizontal_flip=0..1\n",
           okay?"PASS":"FAIL",model);
    return okay;
}
static int check_old_version_bess(GB_model_t model, uint32_t old_version) {
    GB_gameboy_t *a=calloc(1,sizeof(*a)), *b=calloc(1,sizeof(*b));
    uint32_t *sa=calloc(160*144,4), *sb=calloc(160*144,4);
    if (!a || !b || !sa || !sb || !setup(a,model,sa) || !setup(b,model,sb)) return 0;
    size_t size=GB_get_save_state_size(a); unsigned char *snapshot=malloc(size);
    GB_save_state_to_buffer(a,snapshot);
    /* This is an incompatible-version guard test, not an old-layout fixture.
       A genuine v17 fixture is checked separately below. */
    memcpy(snapshot+4,&old_version,sizeof old_version);
    b->display_state=1; b->vram[0x4d6]=0;
    int okay=GB_load_state_from_buffer(b,snapshot,size)==0 &&
             b->version==STRUCT_VERSION && b->display_state!=44 && b->pc==a->pc &&
             b->vram[0x4d6]==0xa5;
    int rejected=GB_load_state_from_buffer(b,snapshot,GB_get_save_state_size_no_bess(a))!=0;
    okay &= rejected;
    printf("%s model=%x synthetic_v%u_tag BESS_fallback=%s no_BESS_rejected=%s\n",
           okay?"PASS":"FAIL",model,old_version,okay?"yes":"no",rejected?"yes":"no");
    free(snapshot); GB_free(a); GB_free(b); free(a);free(b);free(sa);free(sb);
    return okay;
}
static int check_real_v17_bess(const char *path) {
    FILE *file=fopen(path,"rb");
    if (!file) {printf("FAIL cannot open genuine v17 fixture: %s\n",path); return 0;}
    if (fseek(file,0,SEEK_END) || ftell(file)<16) {fclose(file); return 0;}
    size_t size=(size_t)ftell(file); rewind(file);
    unsigned char *snapshot=malloc(size);
    if (!snapshot) {fclose(file); return 0;}
    int read_ok=fread(snapshot,1,size,file)==size; fclose(file);
    uint32_t old_version=0; memcpy(&old_version,snapshot+4,sizeof old_version);
    GB_gameboy_t *g=calloc(1,sizeof(*g)); uint32_t *screen=calloc(160*144,4);
    if (!g || !screen || !setup(g,GB_MODEL_CGB_C,screen)) return 0;
    g->last_tile_data_bus=0x99; g->ram[0x123]=0; g->vram[0x4d6]=0;
    int okay=read_ok && old_version==17 &&
             GB_load_state_from_buffer(g,snapshot,size)==0 &&
             g->version==STRUCT_VERSION && g->display_state!=44 &&
             g->pc==0x100 && g->ram[0x123]==0x94 && g->vram[0x4d6]==0xa5 &&
             g->last_tile_data_bus==0;
    /* The BESS footer records where the portable blocks begin. Truncate
       there to retain the genuine old native layout without a BESS footer. */
    size_t native_size=(size_t)snapshot[size-8] | (size_t)snapshot[size-7]<<8 |
                       (size_t)snapshot[size-6]<<16 | (size_t)snapshot[size-5]<<24;
    int rejected=native_size<size && GB_load_state_from_buffer(g,snapshot,native_size)!=0;
    okay &= rejected;
    printf("%s genuine_v17_layout BESS_fallback=%s WRAM_marker=94 no_BESS_rejected=%s\n",
           okay?"PASS":"FAIL",okay?"yes":"no",rejected?"yes":"no");
    free(snapshot); GB_free(g); free(g); free(screen);
    return okay;
}
int main(int argc, char **argv) {
    if (argc!=2) {fprintf(stderr,"Usage: %s sameboy-v17-cgbc.state\n",argv[0]); return 2;}
    int okay=1; GB_model_t models[]={GB_MODEL_CGB_C,GB_MODEL_CGB_D,GB_MODEL_DMG_B};
    for (unsigned i=0;i<3;i++) {
        okay &= check_model(models[i],44);
        okay &= check_model(models[i],20);
        okay &= check_old_version_bess(models[i],16);
        okay &= check_old_version_bess(models[i],17);
        if (i<2) {
            okay &= check_tile_bus_checkpoint(models[i]);
            okay &= check_batched_tile_bus(models[i]);
        }
    }
    okay &= check_real_v17_bess(argv[1]);
    return okay?0:1;
}
