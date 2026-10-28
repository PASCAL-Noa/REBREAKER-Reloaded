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

    vec2 boxMin = u_previewRect.xy;
    vec2 boxMax = u_previewRect.zw;
    vec2 boxSize = boxMax - boxMin;

    if (boxSize.x > 0.001 && uv.x >= boxMin.x && uv.x <= boxMax.x && uv.y >= boxMin.y && uv.y <= boxMax.y)
    {
        // Inside Preview Monitor: Full authentic CRT shader
        vec2 localUV = (uv - boxMin) / boxSize;

        // CRT curving within preview frame
        vec2 crtUV = localUV * 2.0 - 1.0;
        vec2 offset = crtUV.yx / CURVATURE;
        crtUV += crtUV * offset * offset;
        crtUV = crtUV * 0.5 + 0.5;

        // Outer CRT bevel border if curvature bends outside frame
        if (crtUV.x < 0.0 || crtUV.x > 1.0 || crtUV.y < 0.0 || crtUV.y > 1.0)
        {
            gl_FragColor = vec4(0.01, 0.02, 0.04, 1.0);
            return;
        }

        vec2 edge = smoothstep(0.0, BLUR, crtUV) * (1.0 - smoothstep(1.0 - BLUR, 1.0, crtUV));

        // Authentic dynamic chromatic aberration mapped back to screen space
        float caAmt = 0.004 * (1.0 + max(0.0, u_ca_intensity) * 8.0);
        vec3 color = vec3(
            texture2D(screen_texture, boxMin + ((crtUV - 0.5) * (1.0 + caAmt) + 0.5) * boxSize).r,
            texture2D(screen_texture, boxMin + crtUV * boxSize).g,
            texture2D(screen_texture, boxMin + ((crtUV - 0.5) / (1.0 + caAmt) + 0.5) * boxSize).b
        ) * edge.x * edge.y;

        // CRT Scanlines (identical 3px periodic rhythm to fx.frag)
        if (mod(gl_FragCoord.y, 3.0) < 1.5) color *= 0.75;
        else if (mod(gl_FragCoord.x, 3.0) < 1.0) color *= 0.75;
        else color *= 1.15;

        // Dynamic vignette inside the preview frame
        vec2 vigCoord = (crtUV - 0.5) * vec2(1.25, 1.0);
        float dist = length(vigCoord);
        float vigFactor = smoothstep(0.85, 0.40, dist);
        color = mix(vec3(0.01, 0.02, 0.05), color, vigFactor);

        gl_FragColor = vec4(color, 1.0);
        return;
    }

    // Outside Preview Monitor: Crisp UI text & subtle ambient styling
    vec3 color = texture2D(screen_texture, uv).rgb;

    // Atmospheric arcade vignette for overall menu
    vec2 centerOffset = uv - 0.5;
    float dist = length(centerOffset * vec2(1.25, 1.0));
    float vigFactor = smoothstep(0.95, 0.95 - clamp(u_vignetteIntensity, 0.0, 1.0) * 0.55, dist);
    color = mix(u_vignetteColor, color, vigFactor);

    if (u_flashIntensity > 0.001)
    {
        color += u_flashColor * clamp(u_flashIntensity, 0.0, 1.0);
    }

    // Retain u_ca_intensity uniform across driver shader optimization passes
    color += vec3(u_ca_intensity * 0.00000001);

    gl_FragColor = vec4(color, 1.0);
}
