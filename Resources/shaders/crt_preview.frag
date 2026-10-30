uniform sampler2D u_texture;
uniform float u_time;
uniform float u_switch_progress;
uniform vec2 u_resolution;

const float CURVATURE = 4.2;
const float BLUR = 0.031;

// High-speed pseudo-random hash for retro TV pixel static
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
    vec2 crtUV = uv * 2.0 - 1.0;
    vec2 offset = crtUV.yx / CURVATURE;
    crtUV += crtUV * offset * offset;
    crtUV = crtUV * 0.5 + 0.5;

    // Outer CRT bezel border clipping
    if (crtUV.x < 0.0 || crtUV.x > 1.0 || crtUV.y < 0.0 || crtUV.y > 1.0)
    {
        gl_FragColor = vec4(0.008, 0.012, 0.022, 1.0) * gl_Color;
        return;
    }

    vec2 edge = smoothstep(0.0, BLUR, crtUV) * (1.0 - smoothstep(1.0 - BLUR, 1.0, crtUV));

    // 2. Channel Switch Horizontal Glitch & Sync Tearing
    if (u_switch_progress > 0.001)
    {
        float bandId = floor(crtUV.y * 70.0);
        float bandRand = hash21(vec2(bandId, floor(u_time * 30.0)));
        if (bandRand > 0.65)
        {
            float jitter = (bandRand - 0.82) * 0.14 * u_switch_progress;
            crtUV.x += jitter;
        }

        float wobble = sin(crtUV.y * 22.0 - u_time * 35.0) * 0.018 * u_switch_progress;
        crtUV.x += wobble;
        crtUV.x = clamp(crtUV.x, 0.001, 0.999);
    }

    // 3. Dynamic Chromatic Aberration (Radial RGB Channel Splitting)
    float caAmt = 1.004 + 0.035 * u_switch_progress;
    vec3 color = vec3(
        texture2D(u_texture, (crtUV - 0.5) * caAmt + 0.5).r,
        texture2D(u_texture, crtUV).g,
        texture2D(u_texture, (crtUV - 0.5) / caAmt + 0.5).b
    ) * edge.x * edge.y;

    // 4. Chunky TV Static / Noise during Channel Switch
    if (u_switch_progress > 0.001)
    {
        vec2 pixelCoord = floor((crtUV * u_resolution) / 2.5);
        float noiseSeed = hash21(pixelCoord + vec2(fract(u_time * 173.15), fract(u_time * 841.67)));
        vec3 noiseColor = vec3(noiseSeed);

        // Rolling V-Hold sync bar
        float vRoll = fract(crtUV.y * 0.9 - u_time * 4.0);
        float vBar = smoothstep(0.0, 0.08, vRoll) * (1.0 - smoothstep(0.08, 0.20, vRoll));

        float noiseFactor = clamp(pow(u_switch_progress, 1.2) * 1.15 + vBar * 0.35 * u_switch_progress, 0.0, 1.0);
        color = mix(color, noiseColor, noiseFactor * 0.90);
        color += vec3(0.18 * pow(u_switch_progress, 2.0));
    }

    // 5. CRT Scanlines & Cathode Phosphor Grid (Authentic arcade CRT matching fx.frag)
    if (mod(gl_FragCoord.y, 3.0) < 1.5) color *= 0.75;
    else if (mod(gl_FragCoord.x, 3.0) < 1.0) color *= 0.75;
    else color *= 1.15;

    // 6. Cathode Tube Vignette
    vec2 vigCoord = (crtUV - 0.5) * vec2(1.25, 1.0);
    float dist = length(vigCoord);
    float vigFactor = smoothstep(0.85, 0.40, dist);
    color = mix(vec3(0.01, 0.015, 0.03), color, vigFactor);

    // Subtle phosphor black level lift
    color += vec3(0.012, 0.015, 0.022);

    // Retain u_resolution uniform across driver shader optimization passes
    color += u_resolution.xyx * 0.00000001;

    gl_FragColor = vec4(color, 1.0) * gl_Color;
}
