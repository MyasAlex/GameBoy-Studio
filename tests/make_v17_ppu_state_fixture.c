/* Run only against the preserved v17 core; see fixtures/README.md.
   This deterministic synthetic state contains no commercial ROM or firmware. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gb.h"
#if STRUCT_VERSION != 17
#error The legacy fixture must be produced by the genuine v17 layout.
#endif
static void quiet(GB_gameboy_t *g,const char *text,GB_log_attributes_t flags) {(void)g;(void)text;(void)flags;}
int main(int argc,char **argv) {
    if (argc!=2) return 2;
    GB_random_set_enabled(false);
    GB_gameboy_t *g=calloc(1,sizeof(*g));
    unsigned char rom[32768]={0}; rom[0x143]=0x80;
    if (!g || !GB_init(g,GB_MODEL_CGB_C)) return 1;
    GB_set_log_callback(g,quiet);
    GB_load_rom_from_buffer(g,rom,sizeof rom);
    memset(g->ram,0,g->ram_size); memset(g->vram,0,g->vram_size);
    g->boot_rom_finished=true; g->pc=0x100; g->cgb_mode=true;
    g->last_rtc_second=0;
    g->io_registers[GB_IO_LCDC]=0x87; g->io_registers[GB_IO_STAT]=3;
    g->current_line=68; g->io_registers[GB_IO_LY]=68;
    g->display_state=44; g->display_cycles=0;
    g->cycles_for_line=136; g->position_in_line=24; g->lcd_x=24;
    g->n_visible_objs=1; g->visible_objs[0]=0;
    g->objects_x[0]=32; g->objects_y[0]=80;
    g->mode2_y_bus=0x4c; g->object_flags=0x40; g->during_object_fetch=true;
    g->object_low_line_address=0x120; g->fetcher_state=6; g->bg_fifo.size=8;
    g->vram[0x4d6]=0xa5; g->ram[0x123]=0x94;
    int result=GB_save_state(g,argv[1]);
    printf("version=%u state=%u WRAM[123]=%02x bytes=%zu\n",g->version,g->display_state,g->ram[0x123],GB_get_save_state_size(g));
    GB_free(g); free(g);
    return result?1:0;
}
