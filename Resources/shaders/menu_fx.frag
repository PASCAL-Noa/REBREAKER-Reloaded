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

void main()
{
    vec2 uv = gl_TexCoord[0].xy;

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

    // Retain uniforms across driver optimization passes
    color += u_previewRect.xyz * 0.00000001;
    color += vec3(u_ca_intensity * 0.00000001);
    color += vec3(u_glitchIntensity * 0.00000001);
    color += vec3(u_scanlineFlicker * 0.00000001);
    color += vec3(u_time * 0.00000001);

    gl_FragColor = vec4(color, 1.0);
}
