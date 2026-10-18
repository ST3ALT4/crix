// shaders/bar_visualizer.slang
// Slang shader for array bar visualization in Vulkan

struct UniformData {
    float4x4 projection;
    float time;
    float highlightIntensity;
    float2 padding;
};

[[vk::binding(0, 0)]]
ConstantBuffer<UniformData> globalUniforms;

struct ElementInstance {
    float2 position;
    float2 size;
    float4 color;
    float isMutated;
    float3 pad;
};

[[vk::binding(1, 0)]]
StructuredBuffer<ElementInstance> elements;

struct VSInput {
    float2 vertexPos : POSITION;
};

struct VSOutput {
    float4 position : SV_Position;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float isMutated : TEXCOORD1;
};

[shader("vertex")]
VSOutput vertexMain(VSInput input, uint instanceID : SV_InstanceID) {
    VSOutput output;
    ElementInstance inst = elements[instanceID];

    float2 worldPos = inst.position + input.vertexPos * inst.size;
    output.position = mul(globalUniforms.projection, float4(worldPos, 0.0, 1.0));
    output.color = inst.color;
    output.uv = input.vertexPos;
    output.isMutated = inst.isMutated;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target {
    float4 baseColor = input.color;

    // Subtle edge border effect
    float2 border = smoothstep(float2(0.0, 0.0), float2(0.05, 0.05), input.uv) *
                    smoothstep(float2(0.0, 0.0), float2(0.05, 0.05), float2(1.0, 1.0) - input.uv);
    float edgeFactor = border.x * border.y;

    if (input.isMutated > 0.5) {
        // Highlight pulse effect when element is mutated/swapped
        baseColor.rgb = lerp(baseColor.rgb, float3(1.0, 0.3, 0.2), 0.6);
    }

    return float4(baseColor.rgb * (0.8 + 0.2 * edgeFactor), baseColor.a);
}
