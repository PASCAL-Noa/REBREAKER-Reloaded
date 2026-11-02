uniform float u_progress;
uniform vec4 u_color;
uniform vec2 u_resolution;

// Cyberpunk hash for pseudo-random digital glitch and holographic scan
float hash(vec2 p)
{
    p = fract(p * vec2(443.897, 441.423));
    p += dot(p, p.yx + 19.19);
    return fract(p.x * p.y);
}

void main()
{
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord / u_resolution;

    // 1. Digital Matrix Data Cascade (Torrential vertical holographic streams)
    float time = u_progress * 4.5;
    vec2 streamUV = uv * vec2(40.0, 1.0);
    float colId = floor(streamUV.x);
    float colRand = hash(vec2(colId, 17.3));

    // Downward streaming cyber pulses
    float streamSpeed = 2.5 + colRand * 3.5;
    float streamPhase = fract(uv.y * (3.0 + colRand * 2.0) + time * streamSpeed * 0.4 + colRand);
    float streamBeam = pow(streamPhase, 8.0) * smoothstep(0.0, 0.4, streamPhase);
    streamBeam *= step(0.35, colRand); // sparse column distribution

    // 2. High-Tech Cyber Scanlines & Horizontal Sync
    float scanline = sin(uv.y * u_resolution.y * 1.2 + time * 1.5) * 0.5 + 0.5;
    scanline = pow(scanline, 1.8) * 0.18;

    // 3. Cybernetic Chromatic Glitch Bands
    float glitchTrigger = sin(time * 2.2 + floor(uv.y * 16.0) * 45.1);
    float isGlitch = step(0.92, glitchTrigger) * step(0.4, hash(vec2(floor(time * 8.0), floor(uv.y * 22.0))));
    float glitchShift = (hash(vec2(floor(uv.y * 30.0), floor(time * 12.0))) - 0.5) * 0.08 * isGlitch;

    vec2 glitchedUV = uv + vec2(glitchShift, 0.0);

    // Diagonal Laser Data Grid (Neon cyberspace wireframe aura)
    vec2 diagUV = glitchedUV * vec2(16.0, 9.0) + vec2(time * 0.3, -time * 0.6);
    float gridLine = smoothstep(0.92, 0.98, fract(diagUV.x + diagUV.y)) * 0.25;
    gridLine += smoothstep(0.92, 0.98, fract(diagUV.x - diagUV.y * 0.5)) * 0.20;

    // 4. Cyberpunk Neon Palette (Electric Cyan, Hot Neon Magenta, Cyber Gold)
    vec3 colCyan    = vec3(0.00, 0.94, 1.00); // #00F0FF
    vec3 colMagenta = vec3(1.00, 0.05, 0.60); // #FF007F
    vec3 colGold    = vec3(1.00, 0.78, 0.12); // #FFC700
    vec3 colCore    = vec3(0.92, 1.00, 1.00); // White-cyan high energy

    // Horizontal chromatic split across the streams
    float streamChroma = sin(glitchedUV.x * 24.0 + time * 1.8) * 0.5 + 0.5;
    vec3 beamColor = mix(colCyan, colMagenta, streamChroma);
    beamColor = mix(beamColor, colGold, pow(streamBeam, 2.0));

    // Digital pixel spark flashes
    vec2 pixelGrid = glitchedUV * vec2(64.0, 36.0);
    vec2 pixelId = floor(pixelGrid);
    float sparkRand = hash(pixelId + floor(time * 12.0));
    float spark = step(0.985, sparkRand) * (0.6 + 0.4 * sin(time * 25.0));

    // Combine visual cyber components
    vec3 rgb = beamColor * (streamBeam * 1.4);
    rgb += colMagenta * (gridLine * (0.4 + 0.6 * sin(time + uv.y * 4.0)));
    rgb += colGold * (spark * 1.8);
    rgb += colCyan * scanline;

    // Glitch burst chromatic flash
    if (isGlitch > 0.5)
    {
        rgb += mix(colCyan, colMagenta, step(0.5, uv.x)) * 0.35;
    }

    // Edge CRT vignette
    vec2 vig = uv * (1.0 - uv);
    float vignette = clamp(vig.x * vig.y * 18.0, 0.0, 1.0);
    rgb *= vignette;

    // Modulate with uniform color
    rgb *= u_color.rgb;

    // Alpha channel: Keep subtle so gameplay bricks, ball & paddle remain crisp
    float alpha = (streamBeam * 0.45 + gridLine * 0.25 + spark * 0.75 + isGlitch * 0.22 + scanline * 0.15) * u_color.a;
    alpha = clamp(alpha, 0.0, 0.75);

    gl_FragColor = vec4(rgb, alpha);
}
