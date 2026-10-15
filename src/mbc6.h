#pragma once
/* MBC6's independent 8 KiB ROM/flash and 4 KiB RAM windows.
   Protocol: gbdev Pan Docs MBC6 and Macronix MX29F008TC commands. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef struct {
    uint8_t rom_bank[2],ram_bank[2],flash_select[2],ram_enable,flash_enable,write_enable;
    uint8_t command,unlock,hidden,id_mode,status,protect_sector0,program_count;
    uint32_t program_base;
    uint8_t program[128],ram[32768],flash[1048576],hidden_data[256];
} GBSMBC6;
static void mbc6_reset(GBSMBC6 *m){memset(m,0,sizeof(*m));m->rom_bank[0]=2;m->rom_bank[1]=3;memset(m->ram,0xFF,sizeof(m->ram));memset(m->flash,0xFF,sizeof(m->flash));memset(m->hidden_data,0xFF,sizeof(m->hidden_data));}
static uint32_t mbc6_address(const GBSMBC6 *m,uint16_t addr){return (uint32_t)m->rom_bank[addr>=0x6000]*8192+(addr&8191);}
static uint8_t mbc6_read(GBSMBC6 *m,const uint8_t *rom,size_t size,uint16_t addr){
    if(addr<0x4000)return rom[addr%size];
    if(addr<0x8000){unsigned window=addr>=0x6000;uint32_t at=mbc6_address(m,addr);
        if(!m->flash_select[window])return rom[at%size];if(!m->flash_enable)return 0xFF;
        if(m->id_mode)return (at&255)==0?0xC2:(at&255)==1?0x81:0xFF;
        if(m->status)return (uint8_t)(m->status|(m->protect_sector0?2:0));
        return m->hidden?m->hidden_data[at&255]:m->flash[at&1048575];
    }
    if(addr>=0xA000&&addr<0xC000)return m->ram_enable?m->ram[(unsigned)m->ram_bank[addr>=0xB000]*4096+(addr&4095)]:0xFF;return 0xFF;
}
static void mbc6_flash_write(GBSMBC6 *m,uint32_t at,uint8_t value){
    at&=1048575;
    if(value==0xF0){m->command=m->unlock=m->hidden=m->id_mode=m->status=m->program_count=0;return;}
    if(m->status)return;
    if(m->command==0xA0||m->command==0xE0){
        if(m->program_count<128){if(!m->program_count)m->program_base=at&~127u;
            if((at&~127u)!=m->program_base){m->status=0x90;return;}m->program[at&127]=value;++m->program_count;return;}
        if(at!=(m->program_base+127)){m->status=0x90;return;}
        uint8_t *target=m->command==0xE0?m->hidden_data:m->flash;const uint32_t base=m->command==0xE0?m->program_base&128:m->program_base;
        const int allowed=m->command==0xE0?m->write_enable:(base>=131072||(m->write_enable&&!m->protect_sector0));
        if(allowed)for(unsigned i=0;i<128;++i)target[base+i]&=m->program[i];m->status=allowed?0x80:0x90;return;
    }
    switch(m->unlock){
    case 0:if(at==0x5555&&value==0xAA)m->unlock=1;break;
    case 1:m->unlock=at==0x2AAA&&value==0x55?2:0;break;
    case 2:
        m->unlock=0;if(at!=0x5555)return;
        if(value==0x90)m->id_mode=1;
        else if(value==0xA0){m->command=value;m->program_count=0;memset(m->program,0xFF,128);}
        else if(value==0x80||value==0x60||value==0x77){m->command=value;m->unlock=3;}break;
    case 3:m->unlock=at==0x5555&&value==0xAA?4:0;break;
    case 4:m->unlock=at==0x2AAA&&value==0x55?5:0;break;
    case 5:
        m->unlock=0;
        if(m->command==0x80){
            if(value==0x30){uint32_t sector=at&~131071u;if(sector||(m->write_enable&&!m->protect_sector0))memset(m->flash+sector,0xFF,131072);m->status=0x80;}
            else if(value==0x10&&at==0x5555){unsigned first=m->write_enable&&!m->protect_sector0?0:131072;memset(m->flash+first,0xFF,1048576-first);m->status=0x80;}
        }else if(m->command==0x77&&value==0x77&&at==0x5555)m->hidden=1;
        else if(m->command==0x60&&at==0x5555&&m->write_enable){
            if(value==0x04){memset(m->hidden_data,0xFF,256);m->status=0x80;}
            else if(value==0xE0){m->command=0xE0;m->program_count=0;memset(m->program,0xFF,128);}
            else if(value==0x40||value==0x20){m->protect_sector0=value==0x20;m->status=0x80;}
        }break;
    default:m->unlock=0;break;
    }
}
static void mbc6_write(GBSMBC6 *m,uint16_t addr,uint8_t value){
    if(addr<0x400){m->ram_enable=(value&15)==10;return;}if(addr<0x800){m->ram_bank[0]=value&7;return;}if(addr<0xC00){m->ram_bank[1]=value&7;return;}
    if(addr<0x1000){m->flash_enable=value&1;return;}if(addr==0x1000){m->write_enable=value&1;return;}
    if(addr>=0x2000&&addr<0x2800){m->rom_bank[0]=value&127;return;}if(addr>=0x2800&&addr<0x3000){m->flash_select[0]=(value&8)!=0;return;}
    if(addr>=0x3000&&addr<0x3800){m->rom_bank[1]=value&127;return;}if(addr>=0x3800&&addr<0x4000){m->flash_select[1]=(value&8)!=0;return;}
    if(addr>=0x4000&&addr<0x8000&&m->flash_enable&&m->flash_select[addr>=0x6000])mbc6_flash_write(m,mbc6_address(m,addr),value);
    else if(addr>=0xA000&&addr<0xC000&&m->ram_enable)m->ram[(unsigned)m->ram_bank[addr>=0xB000]*4096+(addr&4095)]=value;
}
