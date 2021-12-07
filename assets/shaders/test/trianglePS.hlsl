
// struct Input
// {
//     float4 pos : SV_POSITION;
//     float4 col : COLOR0;
// };

// float4 main(Input input) : SV_TARGET
// {
//     return float4(1.0f, 0.0f, 1.0f, 1.0f);
// }

float4 main(float4 pos : SV_POSITION) : SV_TARGET
{
    return float4(1.0f, 0.0f, 1.0f, 1.0f);
}