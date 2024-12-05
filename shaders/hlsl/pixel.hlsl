struct ps_in
{
	float4 projPos : SV_POSITION;
	float3 viewPos : POS;
	float3 normal : NORM;
	float2 uv : UV;
	float4 color : COL;
};

struct ps_light
{
    float3 pos;
    float3 ambient;
    float3 diffuse;
    float3 specular;
    float unused1;
    float constant;
	float lin;
	float quadratic;
    float unused2;
};
struct ps_texture
{
    float2 pos;
    float2 scale;
};

#define N_POINTLIGHTS 8
#define N_TEXTURES 8

cbuffer ps_constants : register(b0)
{
    ps_texture tex[N_TEXTURES];
    float4 args1;
    float4 args2;
    float4 color1;
    float4 color2;
}

cbuffer ps_light_constants : register(b1)
{
    ps_light dir_light;
    ps_light point_lights[N_POINTLIGHTS];
    float fog_min;
    float fog_max;
    float unused1;
    float unused2;
    float3 fog_color;
}

SamplerState my_sampler : register(s0);
texture2D base_texture : register(t0);
texture2D texture1 : register(t1);

float4 std2d_main(ps_in input) : SV_TARGET
{
	return base_texture.Sample(my_sampler, input.uv) * input.color;
}


float3 calc_dir_light(ps_light light, float3 normal)
{
    float diffuseStrength = max(0, dot(normal, normalize(light.pos)));
    float3 diffuse = light.diffuse * diffuseStrength;
    return light.ambient + diffuse;
}

float3 calc_point_light(ps_light light, float3 normal, float3 viewPos)
{
    if (light.constant > 0.1)
    {
        float3 light_dir = normalize(light.pos - viewPos);
        float3 color = light.ambient;
        color += light.diffuse * max(0, dot(normal, light_dir));

        float distance = length(light.pos - viewPos);
        float attn = 1.0 / (light.constant + light.lin * distance + light.quadratic * distance * distance);
        return color * attn;
    }
    else
    {
        return float3(0, 0, 0);
    }
}

float3 calc_lights(float3 normal, float3 viewPos)
{
    float3 norm = normalize(normal);
    float3 color = calc_dir_light(dir_light, normal);
    
    for (int i = 0; i < N_POINTLIGHTS; i++)
    {
        color += calc_point_light(point_lights[i], normal, viewPos);
    }
    return color;
}

float3 do_fog(float3 color, float z)
{
    return lerp(color, fog_color, saturate((abs(z) - fog_min) / (fog_max - fog_min)));
}

float4 std3d(ps_in input)
{
    float4 base = base_texture.Sample(my_sampler, input.uv * tex[0].scale + tex[0].pos) * float4(2, 2, 2, 1) * input.color;
    float3 color = base.xyz * calc_lights(input.normal, input.viewPos);
    color = do_fog(color, input.viewPos.z);
    return float4(color, base.w);
}
float4 std3d_main(ps_in input) : SV_TARGET
{
    return std3d(input);
}
float4 std3d_clip_main(ps_in input) : SV_Target
{
    float4 col = std3d(input);
    if (col.w < 0.5)
        discard;
    return float4(col.xyz, 1);
}


float4 post3d_main(ps_in input) : SV_Target
{
    float4 base = base_texture.Sample(my_sampler, input.uv * tex[0].scale + tex[0].pos);
    float4 col = float4(pow(abs(base.xyz), 1.0f / 2.2f) * input.color.xyz, base.w * input.color.w);
    return saturate(col);
}

float4 water_ps(ps_in input) : SV_TARGET
{
    float displacement = texture1.Sample(my_sampler, input.uv * tex[1].scale + tex[1].pos).r * args1.x;
    float4 base = base_texture.Sample(my_sampler, input.uv * tex[0].scale + tex[0].pos + displacement) * float4(2, 2, 2, 1) * input.color;
    float3 color = base.xyz * calc_lights(input.normal, input.viewPos);
    color = do_fog(color, input.viewPos.z);
    return float4(color, base.w);
}