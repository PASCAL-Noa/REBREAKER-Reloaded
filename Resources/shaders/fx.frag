uniform sampler2D screen_texture;

uniform float u_ca_intensity;
uniform float u_vignetteIntensity;
uniform vec3 u_vignetteColor;
uniform float u_flashIntensity;
uniform vec3 u_flashColor;
uniform vec4 u_previewRect;
uniform float u_glitchIntensity;
uniform float u_scanlineFlicker;
uniform float u_time;
uniform float u_alarmIntensity;
uniform vec3 u_alarmColor;

const float CURVATURE = 4.2;
const float BLUR = 0.031;

void main()
{
    vec2 uv = gl_TexCoord[0].xy;

    // CRT curving
    vec2 crtUV = uv * 2.0 - 1.0;
    vec2 offset = crtUV.yx / CURVATURE;
    crtUV += crtUV * offset * offset;
    crtUV = crtUV * 0.5 + 0.5;

    // Horizontal VHS line tears / glitch displacement
    if (u_glitchIntensity > 0.001)
    {
        float tearLine = sin(crtUV.y * 35.0 + u_time * 18.0) * cos(crtUV.y * 8.0 - u_time * 12.0);
        float tearMask = step(0.82, abs(tearLine));
        float glitchOffset = sin(u_time * 36.0 + crtUV.y * 50.0) * 0.020 * u_glitchIntensity * tearMask;
        crtUV.x += glitchOffset;
    }

    vec2 edge = smoothstep(0.0, BLUR, crtUV) * (1.0 - smoothstep(1.0 - BLUR, 1.0, crtUV));

    // Dynamic chromatic aberration split (punchy brief retro bite, quick decay)
    float totalCA = u_ca_intensity + u_glitchIntensity * 0.016;
    float caAmt = 1.0 + max(0.0, totalCA);
    vec3 color = vec3(
        texture2D(screen_texture, (crtUV - 0.5) * caAmt + 0.5).r,
        texture2D(screen_texture, crtUV).g,
        texture2D(screen_texture, (crtUV - 0.5) / caAmt + 0.5).b
    ) * edge.x * edge.y;

    // Soft luma preservation (keeps punch without burning eye strain)
    if (u_glitchIntensity > 0.001)
    {
        float luma = dot(color, vec3(0.299, 0.587, 0.114));
        color = mix(color, vec3(luma), clamp(u_glitchIntensity * 0.20, 0.0, 0.28));
    }

    // CRT Scanlines
    if (mod(gl_FragCoord.y, 3.0) < 1.5) color *= 0.75;
    else if (mod(gl_FragCoord.x, 3.0) < 1.0) color *= 0.75;
    else color *= 1.15;

    // Soft scanline flicker & subtle noise
    if (u_scanlineFlicker > 0.001 || u_glitchIntensity > 0.001)
    {
        float flickerWave = sin(u_time * 16.0) * 0.5 + 0.5;
        float flickerAmt = clamp(u_scanlineFlicker * 0.6 + u_glitchIntensity * 0.4, 0.0, 1.0);
        color *= (1.0 - flickerWave * flickerAmt * 0.12);
    }

    // Dynamic vignette
    vec2 vigCoord = (crtUV - 0.5) * vec2(1.25, 1.0);
    float dist = length(vigCoord);
    float vigFactor = smoothstep(0.85, 0.85 - clamp(u_vignetteIntensity, 0.0, 1.0) * 0.65, dist);
    color = mix(u_vignetteColor, color, vigFactor);

    // Dynamic impact flash
    if (u_flashIntensity > 0.001)
    {
        color += u_flashColor * clamp(u_flashIntensity, 0.0, 1.0);
    }

    // Emergency Red Alarm Light & Strobe Shader
    if (u_alarmIntensity > 0.001)
    {
        // High-frequency buzzer strobe pulse (synchronized with electric door buzzer)
        float strobeWave = sin(u_time * 18.0) * 0.5 + 0.5;
        float alarmPulse = 0.70 + 0.30 * strobeWave;

        // Red alarm perimeter vignette glow
        float alarmBorder = smoothstep(0.38, 0.85, dist) * u_alarmIntensity * alarmPulse;

        // Dynamic horizontal alarm hazard scan-light
        float hazardScan = sin(crtUV.y * 12.0 - u_time * 7.0) * 0.5 + 0.5;
        float scanGlow = pow(hazardScan, 5.0) * 0.35 * u_alarmIntensity;

        // Subtle alarmist high-speed micro-flicker
        float microFlicker = sin(u_time * 50.0) * 0.04 * u_alarmIntensity;

        // Apply emergency red lighting tint to scene
        vec3 redTint = mix(color, color * vec3(1.35, 0.55, 0.55) + u_alarmColor * (0.20 * alarmPulse + scanGlow), u_alarmIntensity * 0.55);
        color = redTint + u_alarmColor * (alarmBorder * 0.95 + microFlicker);
    }

    // Retain u_previewRect uniform across driver shader optimization passes
    color += u_previewRect.xyz * 0.00000001;

    gl_FragColor = vec4(color, 1.0);
}