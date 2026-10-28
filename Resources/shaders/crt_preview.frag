uniform sampler2D u_texture;
uniform float u_time;
uniform float u_switch_progress;
uniform vec2 u_resolution;

// High-speed pseudo-random hash for retro chunky TV pixel static
float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

void main()
{
    vec2 uv = gl_TexCoord[0].xy;

    // 1. CRT Barrel Distortion (Curvature)
    vec2 centered = uv * 2.0 - 1.0;
    vec2 offset = centered.yx / 32.0;
    centered += centered * offset * offset;
    vec2 crtUV = centered * 0.5 + 0.5;

    // Bezel border clipping
    if (crtUV.x < 0.0 || crtUV.x > 1.0 || crtUV.y < 0.0 || crtUV.y > 1.0)
    {
        gl_FragColor = vec4(0.015, 0.02, 0.035, 1.0) * gl_Color;
        return;
    }

    // 2. Channel Switch Horizontal Glitch & Sync Tearing
    if (u_switch_progress > 0.001)
    {
        // Random horizontal jitter bands
        float bandId = floor(crtUV.y * 70.0);
        float bandRand = hash21(vec2(bandId, floor(u_time * 30.0)));
        if (bandRand > 0.65)
        {
            float jitter = (bandRand - 0.82) * 0.14 * u_switch_progress;
            crtUV.x += jitter;
        }

        // Horizontal sine wave wobble
        float wobble = sin(crtUV.y * 22.0 - u_time * 35.0) * 0.018 * u_switch_progress;
        crtUV.x += wobble;

        // Keep inside bounds after tearing
        crtUV.x = clamp(crtUV.x, 0.001, 0.999);
    }

    // 3. Chromatic Aberration (RGB channel splitting)
    float caOffset = 0.002 + 0.022 * u_switch_progress;
    float r = texture2D(u_texture, vec2(clamp(crtUV.x + caOffset, 0.0, 1.0), crtUV.y)).r;
    float g = texture2D(u_texture, crtUV).g;
    float b = texture2D(u_texture, vec2(clamp(crtUV.x - caOffset, 0.0, 1.0), crtUV.y)).b;
    vec3 baseColor = vec3(r, g, b);

    // 4. Chunky Pixel Noise (TV "snow" / static)
    // Quantize into chunky 2.5x2.5 screen pixels using texture resolution
    vec2 pixelCoord = floor((crtUV * u_resolution) / 2.5);
    float noiseSeed = hash21(pixelCoord + vec2(fract(u_time * 173.15), fract(u_time * 841.67)));
    vec3 noiseColor = vec3(noiseSeed);

    // Rolling V-Hold sync bar
    float vRoll = fract(crtUV.y * 0.9 - u_time * 4.0);
    float vBar = smoothstep(0.0, 0.08, vRoll) * (1.0 - smoothstep(0.08, 0.20, vRoll));

    // Dynamic noise intensity curve: strong initial blast, fading smoothly as channel locks in
    float noiseFactor = clamp(pow(u_switch_progress, 1.2) * 1.15 + vBar * 0.35 * u_switch_progress, 0.0, 1.0);
    vec3 finalColor = mix(baseColor, noiseColor, noiseFactor * 0.90);

    // Channel switch brightness burst / phosphor flash
    finalColor += vec3(0.18 * pow(u_switch_progress, 2.0));

    // 5. CRT Scanlines
    float scanline = sin(gl_FragCoord.y * 1.5) * 0.5 + 0.5;
    finalColor *= (0.87 + 0.13 * scanline);

    // 6. Vignette
    vec2 vig = crtUV * (1.0 - crtUV);
    float vignette = clamp(vig.x * vig.y * 22.0, 0.0, 1.0);
    finalColor *= vignette;

    // Retain u_resolution uniform across driver shader optimization passes
    finalColor += u_resolution.xyx * 0.00000001;

    gl_FragColor = vec4(finalColor, 1.0) * gl_Color;
}
