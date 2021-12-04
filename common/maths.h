// NOTE Compile without fast math flags.

#ifdef __FAST_MATH__
YOU SHOULD NOT COMPILE WITH FAST MATH!
#endif

/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to http://unlicense.org/
*/

#ifndef __MATHS_H
#define __MATHS_H

#include "types.h"

#define M_PI 3.14159265358979323846

f64 sqrt(f64 x)
{
    __asm__ ("fsqrt.d %0, %1" : "=f"(x) : "f"(x));
    return x;
}

f32 sqrtf(f32 x)
{
    __asm__ ("fsqrt.s %0, %1" : "=f"(x) : "f"(x));
    return x;
}

typedef union {
	f64 d;
	u64 i;
} ConvertF64Integer;

typedef union {
	f32 f;
	u64 i;
} ConvertF32Integer;

f64 f64ToInteger = 1.0 / 2.22044604925031308085e-16;

f64 floor(f64 x) {
    if(x >= 0.0)
    {
        x =   (f64) ((u64) x);
    }
    else
    {
        x = -((f64) ((u64)(-x))) - 1.0;
    }
    return x;
}

f32 floorF32(f32 x) {
    if(x >= 0.0)
    {
        x =   (f32) ((u64) x);
    }
    else
    {
        x = -((f32) ((u64)(-x))) - 1.0;
    }
    return x;
}

#define D(x) (((ConvertF64Integer) { .i = (x) }).d)
#define F(x) (((ConvertF32Integer) { .i = (x) }).f)

f64 _Sine(f64 x) {
	// Calculates sin(x) for x in [0, pi/4].

	f64 x2 = x * x;

	return x * (D(0x3FF0000000000000) + x2 * (D(0xBFC5555555555540) + x2 * (D(0x3F8111111110ED80) + x2 * (D(0xBF2A01A019AE6000) 
			+ x2 * (D(0x3EC71DE349280000) + x2 * (D(0xBE5AE5DC48000000) + x2 * D(0x3DE5D68200000000)))))));
}

f32 _SineF32(f32 x) {
	// Calculates sin(x) for x in [0, pi/4].

	f32 x2 = x * x;

	return x * (F(0x3F800000) + x2 * (F(0xBE2AAAA0) + x2 * (F(0x3C0882C0) + x2 * F(0xB94C6000))));
}

f64 _arc_sine(f64 x) {
	// Calculates arcsin(x) for x in [0, 0.5].

	f64 x2 = x * x;

	return x * (D(0x3FEFFFFFFFFFFFE6) + x2 * (D(0x3FC555555555FE00) + x2 * (D(0x3FB333333292DF90) + x2 * (D(0x3FA6DB6DFD3693A0) 
			+ x2 * (D(0x3F9F1C608DE51900) + x2 * (D(0x3F96EA0659B9A080) + x2 * (D(0x3F91B4ABF1029100) 
			+ x2 * (D(0x3F8DA8DAF31ECD00) + x2 * (D(0x3F81C01FD5000C00) + x2 * (D(0x3F94BDA038CF6B00)
			+ x2 * (D(0xBF8E849CA75B1E00) + x2 * D(0x3FA146C2D37F2C60))))))))))));
}

f32 _arc_sineF32(f32 x) {
	// Calculates arcsin(x) for x in [0, 0.5].

	f32 x2 = x * x;

	return x * (F(0x3F800004) + x2 * (F(0x3E2AA130) + x2 * (F(0x3D9B2C28) + x2 * (F(0x3D1C1800) + x2 * F(0x3D5A97C0)))));
}

f64 _arc_tangent(f64 x) {
	// Calculates arctan(x) for x in [0, 0.5].

	f64 x2 = x * x;

	return x * (D(0x3FEFFFFFFFFFFFF8) + x2 * (D(0xBFD5555555553B44) + x2 * (D(0x3FC9999999803988) + x2 * (D(0xBFC249248C882E80) 
			+ x2 * (D(0x3FBC71C5A4E4C220) + x2 * (D(0xBFB745B3B75243F0) + x2 * (D(0x3FB3AFAE9A2939E0) 
			+ x2 * (D(0xBFB1030C4A4A1B90) + x2 * (D(0x3FAD6F65C35579A0) + x2 * (D(0xBFA805BCFDAFEDC0)
			+ x2 * (D(0x3F9FC6B5E115F2C0) + x2 * D(0xBF87DCA5AB25BF80))))))))))));
}

f32 _arc_tangentF32(f32 x) {
	// Calculates arctan(x) for x in [0, 0.5].

	f32 x2 = x * x;

	return x * (F(0x3F7FFFF8) + x2 * (F(0xBEAAA53C) + x2 * (F(0x3E4BC990) + x2 * (F(0xBE084A60) + x2 * F(0x3D8864B0)))));
}

f64 _Cosine(f64 x) {
	// Calculates cos(x) for x in [0, pi/4].

	f64 x2 = x * x;

	return D(0x3FF0000000000000) + x2 * (D(0xBFDFFFFFFFFFFFA0) + x2 * (D(0x3FA555555554F7C0) + x2 * (D(0xBF56C16C16475C00) 
			+ x2 * (D(0x3EFA019F87490000) + x2 * (D(0xBE927DF66B000000) + x2 * D(0x3E21B949E0000000))))));
}

f32 _CosineF32(f32 x) {
	// Calculates cos(x) for x in [0, pi/4].

	f32 x2 = x * x;

	return F(0x3F800000) + x2 * (F(0xBEFFFFDA) + x2 * (F(0x3D2A9F60) + x2 * F(0xBAB22C00)));
}

f64 _Tangent(f64 x) {
	// Calculates tan(x) for x in [0, pi/4].

	f64 x2 = x * x;

	return x * (D(0x3FEFFFFFFFFFFFE8) + x2 * (D(0x3FD5555555558000) + x2 * (D(0x3FC1111110FACF90) + x2 * (D(0x3FABA1BA266BFD20) 
			+ x2 * (D(0x3F9664F30E56E580) + x2 * (D(0x3F822703B08BDC00) + x2 * (D(0x3F6D698D2E4A4C00) 
			+ x2 * (D(0x3F57FF4F23EA4400) + x2 * (D(0x3F424F3BEC845800) + x2 * (D(0x3F34C78CA9F61000)
			+ x2 * (D(0xBF042089F8510000) + x2 * (D(0x3F29D7372D3A8000) + x2 * (D(0xBF19D1C5EF6F0000)
			+ x2 * (D(0x3F0980BDF11E8000)))))))))))))));
}

f32 _TangentF32(f32 x) {
	// Calculates tan(x) for x in [0, pi/4].

	f32 x2 = x * x;

	return x * (F(0x3F800001) + x2 * (F(0x3EAAA9AA) + x2 * (F(0x3E08ABA8) + x2 * (F(0x3D58EC90) 
			+ x2 * (F(0x3CD24840) + x2 * (F(0x3AC3CA00) + x2 * F(0x3C272F00)))))));
}

f64 sine(f64 x) {
	u8 negate = 0;

	// x in -infty, infty

	if (x < 0) {
		x = -x;
		negate = 1;
	}

	// x in 0, infty

	x -= 2 * M_PI * floor(x / (2 * M_PI));

	// x in 0, 2*pi

	if (x < M_PI / 2) {
	} else if (x < M_PI) {
		x = M_PI - x;
	} else if (x < 3 * M_PI / 2) {
		x = x - M_PI;
		negate = !negate;
	} else {
		x = M_PI * 2 - x;
		negate = !negate;
	}

	// x in 0, pi/2

	f64 y = x < M_PI / 4 ? _Sine(x) : _Cosine(M_PI / 2 - x);
	return negate ? -y : y;
}

f32 sineF32(f32 x) {
	u8 negate = 0;

	// x in -infty, infty

	if (x < 0) {
		x = -x;
		negate = 1;
	}

	// x in 0, infty

	x -= 2 * M_PI * floorF32(x / (2 * M_PI));

	// x in 0, 2*pi

	if (x < M_PI / 2) {
	} else if (x < M_PI) {
		x = M_PI - x;
	} else if (x < 3 * M_PI / 2) {
		x = x - M_PI;
		negate = !negate;
	} else {
		x = M_PI * 2 - x;
		negate = !negate;
	}

	// x in 0, pi/2

	f32 y = x < M_PI / 4 ? _SineF32(x) : _CosineF32(M_PI / 2 - x);
	return negate ? -y : y;
}

f64 cosine(f64 x) {
	u8 negate = 0;

	// x in -infty, infty

	if (x < 0) {
		x = -x;
	}

	// x in 0, infty

	x -= 2 * M_PI * floor(x / (2 * M_PI));

	// x in 0, 2*pi

	if (x < M_PI / 2) {
	} else if (x < M_PI) {
		x = M_PI - x;
		negate = !negate;
	} else if (x < 3 * M_PI / 2) {
		x = x - M_PI;
		negate = !negate;
	} else {
		x = M_PI * 2 - x;
	}

	// x in 0, pi/2

	f64 y = x < M_PI / 4 ? _Cosine(x) : _Sine(M_PI / 2 - x);
	return negate ? -y : y;
}

f32 cosineF32(f32 x) {
	u8 negate = 0;

	// x in -infty, infty

	if (x < 0) {
		x = -x;
	}

	// x in 0, infty

	x -= 2 * M_PI * floorF32(x / (2 * M_PI));

	// x in 0, 2*pi

	if (x < M_PI / 2) {
	} else if (x < M_PI) {
		x = M_PI - x;
		negate = !negate;
	} else if (x < 3 * M_PI / 2) {
		x = x - M_PI;
		negate = !negate;
	} else {
		x = M_PI * 2 - x;
	}

	// x in 0, pi/2

	f32 y = x < M_PI / 4 ? _CosineF32(x) : _SineF32(M_PI / 2 - x);
	return negate ? -y : y;
}

f64 tangent(f64 x) {
	u8 negate = 0;

	// x in -infty, infty

	if (x < 0) {
		x = -x;
		negate = !negate;
	}

	// x in 0, infty

	x -= M_PI * floor(x / M_PI);

	// x in 0, pi

	if (x > M_PI / 2) {
		x = M_PI - x;
		negate = !negate;
	}

	// x in 0, pi/2

	f64 y = x < M_PI / 4 ? _Tangent(x) : (1.0 / _Tangent(M_PI / 2 - x));
	return negate ? -y : y;
}

f32 tangentF32(f32 x) {
	u8 negate = 0;

	// x in -infty, infty

	if (x < 0) {
		x = -x;
		negate = !negate;
	}

	// x in 0, infty

	x -= M_PI * floorF32(x / M_PI);

	// x in 0, pi

	if (x > M_PI / 2) {
		x = M_PI - x;
		negate = !negate;
	}

	// x in 0, pi/2

	f32 y = x < M_PI / 4 ? _TangentF32(x) : (1.0 / _TangentF32(M_PI / 2 - x));
	return negate ? -y : y;
}

f64 arc_sine(f64 x) {
	u8 negate = 0;

	if (x < 0) { 
		x = -x; 
		negate = 1;
	}

	f64 y;

	if (x < 0.5) {
		y = _arc_sine(x);
	} else {
		y = M_PI / 2 - 2 * _arc_sine(sqrt(0.5 - 0.5 * x));
	}
	
	return negate ? -y : y;
}

f32 arc_sineF32(f32 x) {
	u8 negate = 0;

	if (x < 0) { 
		x = -x; 
		negate = 1;
	}

	f32 y;

	if (x < 0.5) {
		y = _arc_sineF32(x);
	} else {
		y = M_PI / 2 - 2 * _arc_sineF32(sqrtf(0.5 - 0.5 * x));
	}
	
	return negate ? -y : y;
}

f64 arc_cosine(f64 x) {
	return arc_sine(-x) + M_PI / 2;
}

f32 arc_cosineF32(f32 x) {
	return arc_sineF32(-x) + M_PI / 2;
}

f64 arc_tangent(f64 x) {
	u8 negate = 0;

	if (x < 0) { 
		x = -x; 
		negate = 1;
	}

	u8 reciprocalTaken = 0;

	if (x > 1) {
		x = 1 / x;
		reciprocalTaken = 1;
	}

	f64 y;

	if (x < 0.5) {
		y = _arc_tangent(x);
	} else {
		y = 0.463647609000806116 + _arc_tangent((2 * x - 1) / (2 + x));
	}

	if (reciprocalTaken) {
		y = M_PI / 2 - y;
	}
	
	return negate ? -y : y;
}

f32 arc_tangentF32(f32 x) {
	u8 negate = 0;

	if (x < 0) { 
		x = -x; 
		negate = 1;
	}

	u8 reciprocalTaken = 0;

	if (x > 1) {
		x = 1 / x;
		reciprocalTaken = 1;
	}

	f32 y;

	if (x < 0.5f) {
		y = _arc_tangentF32(x);
	} else {
		y = 0.463647609000806116f + _arc_tangentF32((2 * x - 1) / (2 + x));
	}

	if (reciprocalTaken) {
		y = M_PI / 2 - y;
	}
	
	return negate ? -y : y;
}

f64 arc_tangent2(f64 y, f64 x) {
	if (x == 0) return y > 0 ? M_PI / 2 : -M_PI / 2;
	else if (x > 0) return arc_tangent(y / x);
	else if (y >= 0) return M_PI + arc_tangent(y / x);
	else return -M_PI + arc_tangent(y / x);
}

f32 arc_tangent2F32(f32 y, f32 x) {
	if (x == 0) return y > 0 ? M_PI / 2 : -M_PI / 2;
	else if (x > 0) return arc_tangentF32(y / x);
	else if (y >= 0) return M_PI + arc_tangentF32(y / x);
	else return -M_PI + arc_tangentF32(y / x);
}

f64 exponential2(f64 x) {
	f64 a = floor(x * 8);
	int64_t ai = a;

	if (ai < -1024) {
		return 0;
	}

	f64 b = x - a / 8;

	f64 y = D(0x3FF0000000000000) + b * (D(0x3FE62E42FEFA3A00) + b * (D(0x3FCEBFBDFF829140)
			+ b * (D(0x3FAC6B08D73C4A40) + b * (D(0x3F83B2AB53873280) + b * (D(0x3F55D88F363C6C00) 
			+ b * (D(0x3F242C003E4A2000) + b * D(0x3EF0B291F6C00000)))))));

	const f64 m[8] = {
		D(0x3FF0000000000000),
		D(0x3FF172B83C7D517B),
		D(0x3FF306FE0A31B715),
		D(0x3FF4BFDAD5362A27),
		D(0x3FF6A09E667F3BCD),
		D(0x3FF8ACE5422AA0DB),
		D(0x3FFAE89F995AD3AD),
		D(0x3FFD5818DCFBA487),
	};

	y *= m[ai & 7];

	ConvertF64Integer c;
	c.d = y;
	c.i += (ai >> 3) << 52;
	return c.d;
}

f32 exponential2F32(f32 x) {
	f32 a = floorF32(x);
	int32_t ai = a;

	if (ai < -128) {
		return 0;
	}

	f32 b = x - a;

	f32 y = F(0x3F7FFFFE) + b * (F(0x3F31729A) + b * (F(0x3E75E700)
			+ b * (F(0x3D64D520) + b * (F(0x3C128280) + b * F(0x3AF89400)))));

	ConvertF32Integer c;
	c.f = y;
	c.i += ai << 23;
	return c.f;
}

f64 logarithm2(f64 x) {
	ConvertF64Integer c;
	c.d = x;
	int64_t e = ((c.i >> 52) & 2047) - 0x3FF;
	c.i = (c.i & ~(0x7FFL << 52)) + (0x3FFL << 52);
	x = c.d;

	f64 a;

	if (x < 1.125) {
		a = 0;
	} else if (x < 1.250) {
		x *= 1.125 / 1.250;
		a = D(0xBFC374D65D9E608E);
	} else if (x < 1.375) {
		x *= 1.125 / 1.375;
		a = D(0xBFD28746C334FECB);
	} else if (x < 1.500) {
		x *= 1.125 / 1.500;
		a = D(0xBFDA8FF971810A5E);
	} else if (x < 1.625) {
		x *= 1.125 / 1.625;
		a = D(0xBFE0F9F9FFC8932A);
	} else if (x < 1.750) {
		x *= 1.125 / 1.750;
		a = D(0xBFE465D36ED11B11);
	} else if (x < 1.875) {
		x *= 1.125 / 1.875;
		a = D(0xBFE79538DEA712F5);
	} else {
		x *= 1.125 / 2.000;
		a = D(0xBFEA8FF971810A5E);
	}

	f64 y = D(0xC00FF8445026AD97) + x * (D(0x40287A7A02D9353F) + x * (D(0xC03711C58D55CEE2)
			+ x * (D(0x4040E8263C321A26) + x * (D(0xC041EB22EA691BB3) + x * (D(0x403B00FB376D1F10) 
			+ x * (D(0xC02C416ABE857241) + x * (D(0x40138BA7FAA3523A) + x * (D(0xBFF019731AF80316) 
			+ x * D(0x3FB7F1CD3852C200)))))))));

	return y - a + e;
}

f32 logarithm2F32(f32 x) {
	ConvertF32Integer c;
	c.f = x;
	int32_t e = ((c.i >> 23) & 255) - 0x7F;
	c.i = (c.i & ~(0xFF << 23)) + (0x7F << 23);
	x = c.f;

	f64 y = F(0xC05B5154) + x * (F(0x410297C6) + x * (F(0xC1205CEB)
			+ x * (F(0x4114DF63) + x * (F(0xC0C0DBBB) + x * (F(0x402942C6) 
			+ x * (F(0xBF3FF98A) + x * (F(0x3DFE1050) + x * F(0xBC151480))))))));

	return y + e;
}

f64 power(f64 x, f64 y) {
	return exponential2(y * logarithm2(x));
}

f32 powerF32(f32 x, f32 y) {
	return exponential2F32(y * logarithm2F32(x));
}

f64 modulo(f64 x, f64 y) {
	return x - y * floor(x / y);
}

f32 moduloF32(f32 x, f32 y) {
	return x - y * floorF32(x / y);
}

#endif
