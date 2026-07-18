uniform float u_progress;
uniform vec4 u_color;
uniform vec2 u_resolution;

void main()
{
    vec2 uv = gl_FragCoord.xy / u_resolution.xy;

    float aspect = u_resolution.x / u_resolution.y;
    uv.x *= aspect;

    float cells = 25.0;
    vec2 gridUv = fract(uv * cells);
    
    float thickness = 0.04;
    float lineX = step(1.0 - thickness, gridUv.x);
    float lineY = step(1.0 - thickness, gridUv.y);
    float finalGrid = max(lineX, lineY);
    
    float globalFade = smoothstep(0.0, 0.05, u_progress) * smoothstep(1.0, 0.8, u_progress);
    
    float baseIntensity = finalGrid * 0.25 * globalFade;

    float waveFront = u_progress * (aspect + 0.6) - 0.3; 
    
    float dist = waveFront - uv.x;

    float barCore = smoothstep(0.015, 0.0, abs(dist)) * 2.5;
    float barGlow = smoothstep(0.08, 0.0, abs(dist));

    float trail = smoothstep(-0.3, 0.0, dist) * step(dist, 0.0);
    float sweepIntensity = finalGrid * (barGlow + trail * 1.5);

    float intensity = baseIntensity + sweepIntensity + barCore;

    float scanlines = sin(gl_FragCoord.y * 1.5) * 0.15 + 0.85;
    intensity *= scanlines;

    vec4 finalColor = u_color;
    finalColor.rgb *= intensity * 1.5;
    finalColor.a *= intensity;
    
    gl_FragColor = finalColor;
}