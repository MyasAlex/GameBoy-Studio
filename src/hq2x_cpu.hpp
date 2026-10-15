#pragma once
#include "gameboy.hpp"
/* MIT License
*
* Copyright (c) 2015-2023 Lior Halphon
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
/* Based on this (really good) article: http://blog.pkh.me/p/19-butchering-hqx-scaling-filters.html */

/* The colorspace used by the HQnx filters is not really YUV, despite the algorithm description claims it is. It is
   also not normalized. Therefore, we shall call the colorspace used by HQnx "HQ Colorspace" to avoid confusion. */


namespace media {
struct HQColor {float r,g,b;};
inline HQColor hqColor(u32 v){return {(float)((v>>16)&255)/255,(float)((v>>8)&255)/255,(float)(v&255)/255};}
inline HQColor mix2(HQColor a,float x,HQColor b,float y){return {(a.r*x+b.r*y)/(x+y),(a.g*x+b.g*y)/(x+y),(a.b*x+b.b*y)/(x+y)};}
inline HQColor mix3(HQColor a,float x,HQColor b,float y,HQColor c,float z){return {(a.r*x+b.r*y+c.r*z)/(x+y+z),(a.g*x+b.g*y+c.g*z)/(x+y+z),(a.b*x+b.b*y+c.b*z)/(x+y+z)};}
inline bool different(HQColor a,HQColor b){const float r=a.r-b.r,g=a.g-b.g,bl=a.b-b.b;return std::abs((r+g+bl)*0.25f)>0.018f||std::abs((r-bl)*0.25f)>0.002f||std::abs((-r+g*2-bl)*0.125f)>0.005f;}
inline HQColor hqCorner(const std::array<HQColor,9>& w){const auto w0=w[0],w1=w[1],w2=w[2],w3=w[3],w4=w[4],w5=w[5],w6=w[6],w7=w[7],w8=w[8];
#define P(m,r) ((pattern&(m))==(r))
	int pattern = 0;
	if (different(w0, w4)) pattern |= 1;
	if (different(w1, w4)) pattern |= 2;
	if (different(w2, w4)) pattern |= 4;
	if (different(w3, w4)) pattern |= 8;
	if (different(w5, w4)) pattern |= 16;
	if (different(w6, w4)) pattern |= 32;
	if (different(w7, w4)) pattern |= 64;
	if (different(w8, w4)) pattern |= 128;

	if ((P(0xBF,0x37) || P(0xDB,0x13)) && different(w1, w5)) {
		return mix2(w4, 3.0, w3, 1.0);
	}
	if ((P(0xDB,0x49) || P(0xEF,0x6D)) && different(w7, w3)) {
		return mix2(w4, 3.0, w1, 1.0);
	}
	if ((P(0x0B,0x0B) || P(0xFE,0x4A) || P(0xFE,0x1A)) && different(w3, w1)) {
		return w4;
	}
	if ((P(0x6F,0x2A) || P(0x5B,0x0A) || P(0xBF,0x3A) || P(0xDF,0x5A) ||
		 P(0x9F,0x8A) || P(0xCF,0x8A) || P(0xEF,0x4E) || P(0x3F,0x0E) ||
		 P(0xFB,0x5A) || P(0xBB,0x8A) || P(0x7F,0x5A) || P(0xAF,0x8A) ||
		 P(0xEB,0x8A)) && different(w3, w1)) {
		return mix2(w4, 3.0, w0, 1.0);
	}
	if (P(0x0B,0x08)) {
		return mix3(w4, 2.0, w0, 1.0, w1, 1.0);
	}
	if (P(0x0B,0x02)) {
		return mix3(w4, 2.0, w0, 1.0, w3, 1.0);
	}
	if (P(0x2F,0x2F)) {
		return mix3(w4, 4.0, w3, 1.0, w1, 1.0);
	}
	if (P(0xBF,0x37) || P(0xDB,0x13)) {
		return mix3(w4, 5.0, w1, 2.0, w3, 1.0);
	}
	if (P(0xDB,0x49) || P(0xEF,0x6D)) {
		return mix3(w4, 5.0, w3, 2.0, w1, 1.0);
	}
	if (P(0x1B,0x03) || P(0x4F,0x43) || P(0x8B,0x83) || P(0x6B,0x43)) {
		return mix2(w4, 3.0, w3, 1.0);
	}
	if (P(0x4B,0x09) || P(0x8B,0x89) || P(0x1F,0x19) || P(0x3B,0x19)) {
		return mix2(w4, 3.0, w1, 1.0);
	}
	if (P(0x7E,0x2A) || P(0xEF,0xAB) || P(0xBF,0x8F) || P(0x7E,0x0E)) {
		return mix3(w4, 2.0, w3, 3.0, w1, 3.0);
	}
	if (P(0xFB,0x6A) || P(0x6F,0x6E) || P(0x3F,0x3E) || P(0xFB,0xFA) ||
		P(0xDF,0xDE) || P(0xDF,0x1E)) {
		return mix2(w4, 3.0, w0, 1.0);
	}
	if (P(0x0A,0x00) || P(0x4F,0x4B) || P(0x9F,0x1B) || P(0x2F,0x0B) ||
		P(0xBE,0x0A) || P(0xEE,0x0A) || P(0x7E,0x0A) || P(0xEB,0x4B) ||
		P(0x3B,0x1B)) {
		return mix3(w4, 2.0, w3, 1.0, w1, 1.0);
	}

	return mix3(w4, 6.0, w3, 1.0, w1, 1.0);

#undef P
}
inline std::vector<u32> hq2x(std::span<const u32> input,int width,int height){if(width<1||height<1||width>1024||height>1024||input.size()!=(std::size_t)width*height)return {};
    std::vector<u32> out((std::size_t)width*height*4);auto read=[&](int x,int y){return hqColor(input[(std::size_t)std::clamp(y,0,height-1)*width+std::clamp(x,0,width-1)]);};
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)for(int sy=0;sy<2;++sy)for(int sx=0;sx<2;++sx){const int dx=sx?-1:1,dy=sy?-1:1;std::array<HQColor,9> w{};
        for(int row=-1;row<=1;++row)for(int col=-1;col<=1;++col)w[(row+1)*3+col+1]=read(x+col*dx,y+row*dy);const auto c=hqCorner(w);
        auto byte=[](float value){return (u32)std::clamp(std::lround(value*255),0l,255l);};out[(std::size_t)(y*2+sy)*width*2+x*2+sx]=0xFF000000|(byte(c.r)<<16)|(byte(c.g)<<8)|byte(c.b);
    }return out;
}
} // namespace media
