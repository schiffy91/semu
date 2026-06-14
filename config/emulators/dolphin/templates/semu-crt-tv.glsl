/*
[configuration]
[/configuration]
*/

void main()
{
  float2 uv = v_tex0.xy;
  float4 color = texture(samp1, v_tex0);
  float scan = 0.76 + 0.24 * sin(uv.y * GetResolution().y * 3.14159265);
  float mask = 0.92 + 0.08 * sin(uv.x * GetResolution().x * 6.2831853);
  float2 centered = uv * 2.0 - 1.0;
  float vignette = 1.0 - 0.16 * dot(centered, centered);
  color.rgb = pow(color.rgb, float3(1.08)) * scan * mask * vignette;
  SetOutput(color);
}
