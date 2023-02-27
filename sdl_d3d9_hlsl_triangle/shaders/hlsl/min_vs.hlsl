struct VSInputTxVc
{
    float4  Position    : POSITION;
    float2  TexCoord    : TEXCOORD0;
    float4  Color       : COLOR;
};

struct VS_OUTPUT
{
    float4 Position : POSITION;
    float4 Color : COLOR;
};

//Vertex Shader
VS_OUTPUT main(VSInputTxVc VertexIn)
{
    VS_OUTPUT VertexOut;
    VertexOut.Position = VertexIn.Position;

    if( VertexOut.Position.y > 0 )
    {
        VertexOut.Color = float4(1,0,0,1);  // Top half-part must have red
    }
    else
    {
        VertexOut.Color = float4(0,0,1,1); // Bottom half-part must have blue
    }
    //VertexOut.Color = VertexIn.Color;

    return VertexOut;
}
