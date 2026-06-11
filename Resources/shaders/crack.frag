uniform vec4 color;
uniform sampler2D texture;
uniform sampler2D crack_texture;
uniform float crack_amount;

float sdBox( in vec2 p, in vec2 b )
{
    vec2 d = abs(p)-b;
    return length(max(d,0.0)) + min(max(d.x,d.y),0.0);
}

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    uv = (uv - 0.5) * 2.0;

    float d = sdBox(uv, vec2(crack_amount, crack_amount));
    d = 1.0 - step(0.01, d);

    vec4 baseTex = texture2D(texture, gl_TexCoord[0].xy);
    vec4 tex = texture2D(crack_texture, gl_TexCoord[0].xy);
    vec4 baseColor = baseTex * color;

    baseColor.rgb *= (1.0 - crack_amount * 0.4);

    if (d == 0.0)
    {
        gl_FragColor = baseColor;
    } else {
        vec4 dark_color = vec4(baseColor.rgb * 0.3, baseColor.a);
        gl_FragColor = mix(dark_color, baseColor, tex.r);
    }
}
