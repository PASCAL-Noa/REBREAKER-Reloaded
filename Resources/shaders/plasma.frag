uniform float u_progress;
uniform vec4 u_color;
uniform vec2 u_resolution;

// Pseudo-random hash for electric noise
float hash(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

// 2D Noise
float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(mix(hash(i + vec2(0.0, 0.0)), hash(i + vec2(1.0, 0.0)), u.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

void main()
{
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord / u_resolution;

    float t = u_progress * 3.8;

    // 1. Warped Electromagnetic Field (Domain Warping)
    vec2 q = uv - 0.5;
    q.x *= u_resolution.x / u_resolution.y;

    float r = length(q);
    float angle = atan(q.y, q.x);

    // Swirling vortex distortion
    float twist = sin(r * 8.0 - t * 4.0) * 0.35;
    vec2 warpedUV = uv + vec2(cos(angle + twist), sin(angle + twist)) * 0.04;

    // Multi-frequency plasma interference waves
    float v1 = sin(warpedUV.x * 14.0 + t * 2.5);
    float v2 = sin(warpedUV.y * 12.0 - t * 2.0);
    float v3 = sin((warpedUV.x + warpedUV.y) * 10.0 + t * 3.0);
    float v4 = sin(length(warpedUV - 0.5) * 18.0 - t * 4.5);
    float plasmaVal = (v1 + v2 + v3 + v4) * 0.25;

    // 2. High-Voltage Electric Filaments / Lightning Arcs
    vec2 arcUV = uv * vec2(8.0, 6.0) + vec2(t * 0.8, -t * 1.2);
    float n1 = noise(arcUV * 2.5 + vec2(t * 1.5, -t * 2.0));
    float n2 = noise(arcUV * 5.0 - vec2(t * 2.5, t * 1.8));
    float arcLine = abs((arcUV.y + (n1 - 0.5) * 1.2 + (n2 - 0.5) * 0.6) - floor(arcUV.y) - 0.5);
    float lightning = pow(clamp(1.0 - arcLine * 14.0, 0.0, 1.0), 3.0);

    // Secondary vertical high-voltage discharge
    float vArc = abs(sin(uv.x * 24.0 + n1 * 3.0 + t * 5.0));
    float vDischarge = pow(clamp(1.0 - vArc * 8.0, 0.0, 1.0), 4.0) * step(0.65, hash(vec2(floor(t * 10.0), floor(uv.x * 12.0))));

    // 3. Concentric High-Energy Plasma Shockwave Ring
    float waveDist = fract(r * 2.0 - t * 0.6);
    float shockRing = smoothstep(0.08, 0.0, abs(waveDist - 0.5)) * smoothstep(1.2, 0.1, r);

    // 4. Cyber Plasma Color Palette
    vec3 colViolet  = vec3(0.58, 0.15, 0.95); // Deep electric violet
    vec3 colCyan    = vec3(0.00, 0.88, 1.00); // High-voltage cyan
    vec3 colMagenta = vec3(1.00, 0.08, 0.65); // Hot neon magenta
    vec3 colCore    = vec3(0.92, 0.98, 1.00); // White-cyan spark

    // Blend plasma colors based on energy fields
    vec3 rgb = mix(colViolet, colCyan, plasmaVal * 0.5 + 0.5);
    rgb = mix(rgb, colMagenta, pow(clamp(plasmaVal, 0.0, 1.0), 2.0));

    // Add lightning filaments & shockwaves
    rgb += colCyan * (lightning * 1.6);
    rgb += colCore * (vDischarge * 2.0);
    rgb += colMagenta * (shockRing * 0.85);

    // 5. CRT Scanline Interference
    float scanline = sin(uv.y * u_resolution.y * 1.5 + t * 3.0) * 0.5 + 0.5;
    rgb += colCyan * (pow(scanline, 2.5) * 0.12);

    // CRT Edge Vignette
    vec2 vig = uv * (1.0 - uv);
    float vignette = clamp(vig.x * vig.y * 18.0, 0.0, 1.0);
    rgb *= vignette;

    // Modulate with uniform color
    rgb *= u_color.rgb;

    // Alpha channel: Keep ambient so gameplay action stays completely clear
    float energyTotal = abs(plasmaVal) * 0.35 + lightning * 0.75 + vDischarge * 0.85 + shockRing * 0.40;
    float alpha = clamp(energyTotal, 0.0, 0.75) * u_color.a;

    gl_FragColor = vec4(rgb, alpha);
}
