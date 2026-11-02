uniform float u_time;
uniform vec3  u_colorCore;    // White / cyan core
uniform vec3  u_colorInner;   // Electric Blue
uniform vec3  u_colorOuter;   // Deep Violet / Magenta

void main()
{
    vec2 uv = gl_TexCoord[0].xy * 2.0 - 1.0;
    float dist = length(uv);

    if (dist > 1.0)
    {
        discard;
    }

    // Concentric pulsating wave ripples
    float angle = atan(uv.y, uv.x);
    float waves = sin(dist * 18.0 - u_time * 9.0 + sin(angle * 4.0 + u_time * 3.0) * 1.5) * 0.5 + 0.5;
    float plasmaNoise = sin(uv.x * 12.0 + u_time * 5.0) * cos(uv.y * 12.0 - u_time * 4.0);

    // Color gradient: Core (white/cyan) -> Inner (blue) -> Outer (violet)
    vec3 color = mix(u_colorInner, u_colorOuter, smoothstep(0.2, 0.85, dist + plasmaNoise * 0.1));
    color = mix(u_colorCore, color, smoothstep(0.0, 0.45, dist));
    color += u_colorInner * waves * (1.0 - dist) * 0.6;

    // Edge softness & glowing border
    float alpha = smoothstep(1.0, 0.75, dist);
    float rim = smoothstep(0.7, 0.98, dist) * (0.8 + 0.2 * sin(u_time * 10.0));
    color += u_colorInner * rim * 1.4;

    gl_FragColor = vec4(color, alpha);
}
