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
static int check_old_version_bess(GB_model_t model) {
    GB_gameboy_t *a=calloc(1,sizeof(*a)), *b=calloc(1,sizeof(*b));
    uint32_t *sa=calloc(160*144,4), *sb=calloc(160*144,4);
    if (!a || !b || !sa || !sb || !setup(a,model,sa) || !setup(b,model,sb)) return 0;
    size_t size=GB_get_save_state_size(a); unsigned char *snapshot=malloc(size);
    GB_save_state_to_buffer(a,snapshot);
    uint32_t old_version=16; memcpy(snapshot+4,&old_version,sizeof old_version);
    b->display_state=1; b->vram[0x4d6]=0;
    int okay=GB_load_state_from_buffer(b,snapshot,size)==0 &&
             b->version==17 && b->display_state!=44 && b->pc==a->pc &&
             b->vram[0x4d6]==0xa5;
    printf("%s model=%x native_v16_import_via_BESS=%s restored_state=%d\n",
           okay?"PASS":"FAIL",model,okay?"yes":"no",b->display_state);
    free(snapshot); GB_free(a); GB_free(b); free(a);free(b);free(sa);free(sb);
    return okay;
}
int main(void) {
    int okay=1; GB_model_t models[]={GB_MODEL_CGB_C,GB_MODEL_CGB_D,GB_MODEL_DMG_B};
    for (unsigned i=0;i<3;i++) {
        okay &= check_model(models[i],44);
        okay &= check_model(models[i],20);
        okay &= check_old_version_bess(models[i]);
    }
    return okay?0:1;
}


