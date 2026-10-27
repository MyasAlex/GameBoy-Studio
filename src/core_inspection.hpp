#pragma once
#include <cstdint>

// Studio's debugger view. This is data only; no external engine ABI or loader.
struct GBSInspect {
    std::uint8_t fetcher,mode,bg_size,obj_size,bg_read,obj_read,window,selected_objects;
    std::int16_t x,window_y;
    std::uint8_t tile,attributes,bg_pixels[16],obj_pixels[16],obj_priority[16];
    std::uint8_t active[4],sample[4],volume[4];
    std::uint16_t length[4],period[4],countdown[4];
};
