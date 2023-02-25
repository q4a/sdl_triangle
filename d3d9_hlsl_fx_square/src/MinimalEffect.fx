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
VS_OUTPUT RenderSceneVS(VSInputTxVc VertexIn)
{
    VS_OUTPUT VertexOut;
    VertexOut.Position = VertexIn.Position;

    if( VertexOut.Position.x > 0 )
    {
        VertexOut.Color = float4(1,0,0,1);  // Right half-part must have red color
    }
    else
    {
        VertexOut.Color = float4(0,0,1,1); // Left half-part must have blue color
    }

    return VertexOut;
}

//Pixel Shader
float4 RenderScenePS(float4 Color : COLOR) : COLOR
{
    return Color;
}

technique RenderScene
{
    pass P0
    { 
        VertexShader  = (compile vs_1_1 RenderSceneVS());
        PixelShader  = (compile ps_2_0 RenderScenePS());
    }
}
