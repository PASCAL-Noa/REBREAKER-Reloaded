uniform float u_time;

void main()
{
    vec2 uv = gl_TexCoord[0].xy * 2.0 - 1.0;
    float dist = length(uv);

    if (dist > 1.0)
    {
        discard;
    }

    // Sphere normal mapping
    float z = sqrt(1.0 - dist * dist);
    vec3 normal = vec3(uv.x, uv.y, z);

    // Grid coordinates on sphere (mirror facets)
    float rot = u_time * 1.5;
    float lon = atan(normal.x, normal.z) + rot;
    float lat = asin(normal.y);

    float facetGridX = sin(lon * 24.0);
    float facetGridY = sin(lat * 18.0);
    float facetBorder = step(0.85, abs(facetGridX)) + step(0.85, abs(facetGridY));

    // Specular reflection shimmer per facet
    float facetId = floor(lon * 24.0 / 3.14159) * 17.0 + floor(lat * 18.0 / 3.14159) * 31.0;
    float shimmer = sin(facetId + u_time * 8.0) * 0.5 + 0.5;

    // Disco rainbow tint
    float hue = mod(lon * 0.5 + u_time * 0.8, 1.0);
    vec3 rainbow = 0.5 + 0.5 * cos(6.28318 * (hue + vec3(0.0, 0.33, 0.67)));

    vec3 mirrorColor = mix(vec3(0.85, 0.92, 1.0), rainbow, 0.45);
    mirrorColor += vec3(1.0, 1.0, 1.0) * pow(shimmer, 6.0) * 1.8;

    // Dark facet edges
    if (facetBorder > 0.5)
    {
        mirrorColor *= 0.25;
    }

    // Sphere rim shading
    float rim = pow(1.0 - z, 3.0);
    mirrorColor += rainbow * rim * 1.5;

    float alpha = smoothstep(1.0, 0.92, dist);
    gl_FragColor = vec4(mirrorColor, alpha);
}
