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

    // Retain u_previewRect uniform across driver shader optimization passes
    color += u_previewRect.xyz * 0.00000001;

    gl_FragColor = vec4(color, 1.0);
}