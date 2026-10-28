uniform sampler2D screen_texture;

uniform float u_ca_intensity;
uniform float u_vignetteIntensity;
uniform vec3 u_vignetteColor;
uniform float u_flashIntensity;
uniform vec3 u_flashColor;
uniform vec4 u_previewRect;

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

    vec2 edge = smoothstep(0.0, BLUR, crtUV) * (1.0 - smoothstep(1.0 - BLUR, 1.0, crtUV));

    // Dynamic chromatic aberration
    float caAmt = 1.0 + max(0.0, u_ca_intensity);
    vec3 color = vec3(
        texture2D(screen_texture, (crtUV - 0.5) * caAmt + 0.5).r,
        texture2D(screen_texture, crtUV).g,
        texture2D(screen_texture, (crtUV - 0.5) / caAmt + 0.5).b
    ) * edge.x * edge.y;

    // CRT Scanlines
    if (mod(gl_FragCoord.y, 3.0) < 1.5) color *= 0.75;
    else if (mod(gl_FragCoord.x, 3.0) < 1.0) color *= 0.75;
    else color *= 1.15;

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