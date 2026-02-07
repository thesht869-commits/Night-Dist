#version 300 es
precision highp float;

uniform sampler2D tex;
uniform float radius;
uniform vec2 halfpixel;
uniform float passSaturation;
uniform float passBrightness;

in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main() {
    vec2 uv = v_texcoord / 2.0;

    vec4 sum = texture(tex, uv + vec2(-halfpixel.x * 2.0, 0.0) * radius);

    sum += texture(tex, uv + vec2(-halfpixel.x,  halfpixel.y) * radius) * 2.0;
    sum += texture(tex, uv + vec2(0.0,           halfpixel.y * 2.0) * radius);
    sum += texture(tex, uv + vec2(halfpixel.x,   halfpixel.y) * radius) * 2.0;
    sum += texture(tex, uv + vec2(halfpixel.x * 2.0, 0.0) * radius);
    sum += texture(tex, uv + vec2(halfpixel.x,  -halfpixel.y) * radius) * 2.0;
    sum += texture(tex, uv + vec2(0.0,          -halfpixel.y * 2.0) * radius);
    sum += texture(tex, uv + vec2(-halfpixel.x, -halfpixel.y) * radius) * 2.0;

    vec4 color = sum / 12.0;

    // simple HSL conversion to allow saturation/brightness tweaks per pass
    // from blur1.frag
    float red   = color.r;
    float green = color.g;
    float blue  = color.b;

    float minc  = min(color.r, min(color.g, color.b));
    float maxc  = max(color.r, max(color.g, color.b));
    float delta = maxc - minc;

    float lum = (minc + maxc) * 0.5;
    float sat = 0.0;
    float hue = 0.0;

    if (lum > 0.0 && lum < 1.0) {
        float mul = (lum < 0.5) ? (lum) : (1.0 - lum);
        sat       = delta / (mul * 2.0);
    }

    if (delta > 0.0) {
        vec3  maxcVec = vec3(maxc);
        vec3  masks = vec3(equal(maxcVec, vec3(red, green, blue))) * vec3(notEqual(maxcVec, vec3(green, blue, red)));
        vec3  adds = vec3(0.0, 2.0, 4.0) + vec3(green - blue, blue - red, red - green) / delta;

        hue += dot(adds, masks);
        hue /= 6.0;

        if (hue < 0.0)
            hue += 1.0;
    }

    // apply multipliers
    sat = clamp(sat * passSaturation, 0.0, 1.0);
    lum = clamp(lum * passBrightness, 0.0, 1.0);

    // convert back to RGB (hsl2rgb from blur1.frag)
    const float onethird = 1.0 / 3.0;
    const float twothird = 2.0 / 3.0;
    const float rcpsixth = 6.0;

    float       hue2 = hue;
    float       sat2 = sat;
    float       lum2 = lum;

    vec3        xt = vec3(0.0);

    if (hue2 < onethird) {
        xt.r = rcpsixth * (onethird - hue2);
        xt.g = rcpsixth * hue2;
        xt.b = 0.0;
    } else if (hue2 < twothird) {
        xt.r = 0.0;
        xt.g = rcpsixth * (twothird - hue2);
        xt.b = rcpsixth * (hue2 - onethird);
    } else
        xt = vec3(rcpsixth * (hue2 - twothird), 0.0, rcpsixth * (1.0 - hue2));

    xt = min(xt, 1.0);

    float sat2d   = 2.0 * sat;
    float satinv = 1.0 - sat;
    float luminv = 1.0 - lum2;
    float lum2m1 = (2.0 * lum2) - 1.0;
    vec3  ct     = (sat2d * xt) + satinv;

    vec3  rgb;
    if (lum2 >= 0.5)
        rgb = (luminv * ct) + lum2m1;
    else
        rgb = lum2 * ct;

    fragColor = vec4(rgb, color.a);
}
