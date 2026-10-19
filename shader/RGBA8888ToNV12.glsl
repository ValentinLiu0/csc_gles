#version 310 es

precision highp float;
precision highp int;

layout(local_size_x = 8, local_size_y = 8) in;

uniform sampler2D tex_src;
uniform int in_width;
uniform int in_height;

// NV12: Y plane (w*h bytes), then interleaved UV plane (w*h/2 bytes).
layout(std430, binding = 0) writeonly buffer NV12Out {
    uint words[];
};

// BT.2020 NCL: full-range R'G'B' -> Y'CbCr, Cb/Cr centered at 0.
const mat3 RGB2YUV = mat3(
    0.2627, -0.139630,  0.500000,
    0.6780, -0.360370, -0.459786,
    0.0593,  0.500000, -0.040214
);

vec3 yuvAt(ivec2 p) {
    return RGB2YUV * texelFetch(tex_src, p, 0).rgb;
}

uint pack4(uint a, uint b, uint c, uint d) {
    return a | (b << 8) | (c << 16) | (d << 24);
}

uint y8(float y) {
    return uint(clamp(16.0 + 219.0 * y, 16.0, 235.0) + 0.5);
}

uint c8(float c) {
    return uint(clamp(128.0 + 224.0 * c, 16.0, 240.0) + 0.5);
}

void main() {
    // One invocation handles a 4x2 luma block:
    // 2 Y words + 1 UV word.
    uvec2 p = gl_GlobalInvocationID.xy * uvec2(4u, 2u);
    uint w = uint(in_width);
    uint h = uint(in_height);
    if (p.x + 4u > w || p.y + 2u > h) return;

    ivec2 q = ivec2(p);
    vec3 a00 = yuvAt(q + ivec2(0, 0));
    vec3 a01 = yuvAt(q + ivec2(1, 0));
    vec3 a02 = yuvAt(q + ivec2(2, 0));
    vec3 a03 = yuvAt(q + ivec2(3, 0));
    vec3 a10 = yuvAt(q + ivec2(0, 1));
    vec3 a11 = yuvAt(q + ivec2(1, 1));
    vec3 a12 = yuvAt(q + ivec2(2, 1));
    vec3 a13 = yuvAt(q + ivec2(3, 1));

    // Y plane, 4 bytes per word.
    uint y0 = pack4(y8(a00.x), y8(a01.x), y8(a02.x), y8(a03.x));
    uint y1 = pack4(y8(a10.x), y8(a11.x), y8(a12.x), y8(a13.x));

    uint ybytes = w * h;
    uint yi = (p.y * w + p.x) >> 2;
    words[yi] = y0;
    words[yi + (w >> 2)] = y1;

    // UV plane: average Cb/Cr over each 2x2 luma block.
    float u0 = (a00.y + a01.y + a10.y + a11.y) * 0.25;
    float v0 = (a00.z + a01.z + a10.z + a11.z) * 0.25;
    float u1 = (a02.y + a03.y + a12.y + a13.y) * 0.25;
    float v1 = (a02.z + a03.z + a12.z + a13.z) * 0.25;

    // NV12 byte order: U0,V0,U1,V1.
    uint uv = pack4(c8(u0), c8(v0), c8(u1), c8(v1));
    uint uvi = (ybytes + ((p.y >> 1) * w + p.x)) >> 2;
    words[uvi] = uv;
}