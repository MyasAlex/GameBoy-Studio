#include "../src/accuracy_api.h"
#include "../third_party/sameboy/Core/model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL %s line %d\n",#x,__LINE__);return 0;} } while(0)

/* Original synthetic cartridge and firmware.  No manufacturer image is used. */
static uint8_t cartridge[32768];
static int check_default(const GBSApi *a,unsigned model){
    const GBSCallbacks callbacks={0};void *engine=a->create(&callbacks,NULL);CHECK(engine);
    CHECK(a->reset(engine,model,cartridge,sizeof cartridge,NULL,0,NULL,0));
    GBSView view;a->view(engine,&view);
    CHECK(!view.boot&&view.pc==0x100&&view.sp==0xFFFE);
    CHECK(view.a==(model==GB_MODEL_MGB?0xFF:1));
    CHECK(view.div==0xABCC&&a->read(engine,0xFF04)==0xAB);
    CHECK(a->read(engine,0xFF44)==0&&a->read(engine,0xFF41)==0x85);
    CHECK(a->read(engine,0xFF26)==0xF1);
    /* Saving at the cartridge boundary preserves the peripheral phase through
       the first VBlank exit and a serial transfer, including its clock edge. */
    a->write(engine,0xFF01,0xA5);a->write(engine,0xFF02,0x81);
    size_t size=a->state_size(engine);uint8_t *before=malloc(size),*future=malloc(size),*replay=malloc(size);
    CHECK(before&&future&&replay);a->save_state(engine,before);
    for(unsigned i=0;i<2000;i++)a->step(engine);
    a->save_state(engine,future);CHECK(!(a->read(engine,0xFF02)&0x80));
    CHECK(a->load_state(engine,before,size));
    for(unsigned i=0;i<2000;i++)a->step(engine);
    a->save_state(engine,replay);CHECK(!memcmp(future,replay,size));
    CHECK(a->reset(engine,model,cartridge,sizeof cartridge,NULL,0,NULL,0));
    a->view(engine,&view);CHECK(view.div==0xABCC&&a->read(engine,0xFF41)==0x85);
    free(before);free(future);free(replay);a->destroy(engine);
    printf("PASS model=%x replacement handoff, reset and serial snapshot continuation\n",model);return 1;
}
static int check_external(const GBSApi *a){
    const GBSCallbacks callbacks={0};void *engine=a->create(&callbacks,NULL);CHECK(engine);
    uint8_t boot[256]={0};
    boot[0]=0x31;boot[1]=0xFE;boot[2]=0xFF; /* LD SP,FFFE */
    boot[3]=0x3E;boot[4]=0x43;              /* LD A,43 (bit 0 unmaps firmware) */
    boot[5]=0xC3;boot[6]=0xFE;             /* JP 00FE */
    boot[0xFE]=0xE0;boot[0xFF]=0x50;       /* LDH (FF50),A */
    CHECK(a->reset(engine,GB_MODEL_DMG_B,cartridge,sizeof cartridge,boot,sizeof boot,NULL,0));
    GBSView view;a->view(engine,&view);CHECK(view.boot&&view.pc==0);
    for(unsigned i=0;i<4;i++)a->step(engine);
    a->view(engine,&view);CHECK(!view.boot&&view.pc==0x100&&view.a==0x43);
    CHECK(view.div<0x100&&a->read(engine,0xFF40)==0);
    a->destroy(engine);puts("PASS supplied firmware retains its own DIV/LCD handoff");return 1;
}
static int check_mbc6_overlay(const GBSApi *a){
    /* MBC6 reads must not hide the internal DMG/CGB boot mapping. */
    cartridge[0x147]=0x20;cartridge[0x149]=3;
    CHECK(check_default(a,GB_MODEL_DMG_B)&&check_external(a));
    const GBSCallbacks callbacks={0};void *engine=a->create(&callbacks,NULL);CHECK(engine);
    uint8_t boot[0x900]={0};
    boot[0]=0xC3;boot[1]=0;boot[2]=2; /* JP 0200 */
    boot[0x200]=0x3E;boot[0x201]=0x43; /* LD A,43 */
    boot[0x202]=0xC3;boot[0x203]=0xFE; /* JP 00FE */
    boot[0xFE]=0xE0;boot[0xFF]=0x50;
    cartridge[0x100]=0xA5;cartridge[0x200]=0x5A;
    CHECK(a->reset(engine,GB_MODEL_CGB_E,cartridge,sizeof cartridge,boot,sizeof boot,NULL,0));
    CHECK(a->read(engine,0)==0xC3&&a->read(engine,0x200)==0x3E);
    CHECK(a->read(engine,0x100)==0xA5); /* Header hole comes from the cartridge. */
    for(unsigned i=0;i<4;i++)a->step(engine);
    GBSView view;a->view(engine,&view);CHECK(!view.boot&&view.pc==0x100&&view.a==0x43);
    CHECK(a->read(engine,0)==0&&a->read(engine,0x200)==0x5A);
    a->destroy(engine);memset(cartridge,0,sizeof cartridge);
    puts("PASS MBC6 respects default, DMG and two-segment CGB boot overlays");return 1;
}
int main(void){
    const GBSApi *a=gbs_api_v1();
    if(!a||!check_default(a,GB_MODEL_DMG_B)||!check_default(a,GB_MODEL_MGB)||!check_external(a)||!check_mbc6_overlay(a))return 1;
    return 0;
}
