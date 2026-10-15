#pragma once
static constexpr const char* hq2x_shader=R"HQX(
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

float3 rgb_to_hq_colospace(float4 rgb)
{
	return float3( 0.250 * rgb.r + 0.250 * rgb.g + 0.250 * rgb.b,
				 0.250 * rgb.r - 0.000 * rgb.g - 0.250 * rgb.b,
				-0.125 * rgb.r + 0.250 * rgb.g - 0.125 * rgb.b);
}

bool is_different(float4 a, float4 b)
{
	float3 diff = abs(rgb_to_hq_colospace(a) - rgb_to_hq_colospace(b));
	return diff.x > 0.018 || diff.y > 0.002 || diff.z > 0.005;
}

#define P(m, r) ((pattern & (m)) == (r))

float4 interp_2px(float4 c1, float w1, float4 c2, float w2)
{
	return (c1 * w1 + c2 * w2) / (w1 + w2);
}

float4 interp_3px(float4 c1, float w1, float4 c2, float w2, float4 c3, float w3)
{
	return (c1 * w1 + c2 * w2 + c3 * w3) / (w1 + w2 + w3);
}

float4 hq2x(float2 position, float2 input_resolution)
{
	// o = offset, the width of a pixel
	float2 o = float2(1, 1) / input_resolution;

	/* We always calculate the top left pixel.  If we need a different pixel, we flip the image */

	// p = the position within a pixel [0...1]
	float2 p = frac(position * input_resolution);

	if (p.x > 0.5) o.x = -o.x;
	if (p.y > 0.5) o.y = -o.y;

	float4 w0 = frame.Sample(pointSample, position + float2( -o.x, -o.y));
	float4 w1 = frame.Sample(pointSample, position + float2(    0, -o.y));
	float4 w2 = frame.Sample(pointSample, position + float2(  o.x, -o.y));
	float4 w3 = frame.Sample(pointSample, position + float2( -o.x,    0));
	float4 w4 = frame.Sample(pointSample, position + float2(    0,    0));
	float4 w5 = frame.Sample(pointSample, position + float2(  o.x,    0));
	float4 w6 = frame.Sample(pointSample, position + float2( -o.x,  o.y));
	float4 w7 = frame.Sample(pointSample, position + float2(    0,  o.y));
	float4 w8 = frame.Sample(pointSample, position + float2(  o.x,  o.y));

	int pattern = 0;
	if (is_different(w0, w4)) pattern |= 1;
	if (is_different(w1, w4)) pattern |= 2;
	if (is_different(w2, w4)) pattern |= 4;
	if (is_different(w3, w4)) pattern |= 8;
	if (is_different(w5, w4)) pattern |= 16;
	if (is_different(w6, w4)) pattern |= 32;
	if (is_different(w7, w4)) pattern |= 64;
	if (is_different(w8, w4)) pattern |= 128;

	if ((P(0xBF,0x37) || P(0xDB,0x13)) && is_different(w1, w5)) {
		return interp_2px(w4, 3.0, w3, 1.0);
	}
	if ((P(0xDB,0x49) || P(0xEF,0x6D)) && is_different(w7, w3)) {
		return interp_2px(w4, 3.0, w1, 1.0);
	}
	if ((P(0x0B,0x0B) || P(0xFE,0x4A) || P(0xFE,0x1A)) && is_different(w3, w1)) {
		return w4;
	}
	if ((P(0x6F,0x2A) || P(0x5B,0x0A) || P(0xBF,0x3A) || P(0xDF,0x5A) ||
		 P(0x9F,0x8A) || P(0xCF,0x8A) || P(0xEF,0x4E) || P(0x3F,0x0E) ||
		 P(0xFB,0x5A) || P(0xBB,0x8A) || P(0x7F,0x5A) || P(0xAF,0x8A) ||
		 P(0xEB,0x8A)) && is_different(w3, w1)) {
		return interp_2px(w4, 3.0, w0, 1.0);
	}
	if (P(0x0B,0x08)) {
		return interp_3px(w4, 2.0, w0, 1.0, w1, 1.0);
	}
	if (P(0x0B,0x02)) {
		return interp_3px(w4, 2.0, w0, 1.0, w3, 1.0);
	}
	if (P(0x2F,0x2F)) {
		return interp_3px(w4, 4.0, w3, 1.0, w1, 1.0);
	}
	if (P(0xBF,0x37) || P(0xDB,0x13)) {
		return interp_3px(w4, 5.0, w1, 2.0, w3, 1.0);
	}
	if (P(0xDB,0x49) || P(0xEF,0x6D)) {
		return interp_3px(w4, 5.0, w3, 2.0, w1, 1.0);
	}
	if (P(0x1B,0x03) || P(0x4F,0x43) || P(0x8B,0x83) || P(0x6B,0x43)) {
		return interp_2px(w4, 3.0, w3, 1.0);
	}
	if (P(0x4B,0x09) || P(0x8B,0x89) || P(0x1F,0x19) || P(0x3B,0x19)) {
		return interp_2px(w4, 3.0, w1, 1.0);
	}
	if (P(0x7E,0x2A) || P(0xEF,0xAB) || P(0xBF,0x8F) || P(0x7E,0x0E)) {
		return interp_3px(w4, 2.0, w3, 3.0, w1, 3.0);
	}
	if (P(0xFB,0x6A) || P(0x6F,0x6E) || P(0x3F,0x3E) || P(0xFB,0xFA) ||
		P(0xDF,0xDE) || P(0xDF,0x1E)) {
		return interp_2px(w4, 3.0, w0, 1.0);
	}
	if (P(0x0A,0x00) || P(0x4F,0x4B) || P(0x9F,0x1B) || P(0x2F,0x0B) ||
		P(0xBE,0x0A) || P(0xEE,0x0A) || P(0x7E,0x0A) || P(0xEB,0x4B) ||
		P(0x3B,0x1B)) {
		return interp_3px(w4, 2.0, w3, 1.0, w1, 1.0);
	}

	return interp_3px(w4, 6.0, w3, 1.0, w1, 1.0);
}


)HQX";
