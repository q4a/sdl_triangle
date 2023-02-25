
#include "d3dUtility.h"

//
// Globals
//

IDirect3DDevice9* Device = 0;

const int Width  = 640;
const int Height = 480;

IDirect3DVertexBuffer9* Triangle = 0;
IDirect3DVertexShader9* ShaderVS = 0;
IDirect3DPixelShader9* ShaderPS = 0;

//
// Classes and Structures
//

struct Vertex
{
    Vertex() {}

    Vertex(float x, float y, float z, uint32_t color)
    {
        _x = x;	 _y = y;  _z = z; _color = color;
    }

    float _x, _y, _z;
    uint32_t _color;

    static const uint32_t FVF;
};
const uint32_t Vertex::FVF = D3DFVF_XYZ | D3DFVF_DIFFUSE;

//
// Framework Functions
//
bool Setup()
{
    //
    // Create the vertex buffer.
    //

    if (FAILED(Device->CreateVertexBuffer(
        3 * sizeof(Vertex), // size in bytes
        D3DUSAGE_WRITEONLY, // flags
        Vertex::FVF,        // vertex format
        D3DPOOL_MANAGED,    // managed memory pool
        &Triangle,          // return create vertex buffer
        0)                  // not used - set to 0
    ))
        return false;

    //
    // Fill the buffers with the triangle data.
    //

    Vertex* pVertices;
    if (FAILED(Triangle->Lock(0, 0, (void**)&pVertices, 0)))
        return false;

    pVertices[0] = Vertex(-1.0f, -1.0f, 0.0f, 0xff0000ff);
    pVertices[1] = Vertex( 0.0f,  1.0f, 0.0f, 0xff00ff00);
    pVertices[2] = Vertex( 1.0f, -1.0f, 0.0f, 0xffff0000);

    Triangle->Unlock();

    //
    // Compile shader.
    //

    HRESULT hr = 0;
    ID3DXBuffer* shader = 0;
    ID3DXBuffer* errorBuffer = 0;

    hr = D3DXCompileShaderFromFile(
        // path from build/msvc-.../bin
        "../../../src/min.vs",
        0,
        0,
        "Main",  // entry point function name
        "vs_1_1",// shader version to compile to
        D3DXSHADER_DEBUG | D3DXSHADER_USE_LEGACY_D3DX9_31_DLL,
        &shader,
        &errorBuffer,
        0);

    // output any error messages
    if (errorBuffer)
    {
        ::MessageBox(0, (char*)errorBuffer->GetBufferPointer(), 0, 0);
        d3d::Release<ID3DXBuffer*>(errorBuffer);
    }

    if (FAILED(hr))
    {
        ::MessageBox(0, "D3DXCreateEffectFromFile() - FAILED for VS", 0, 0);
        return false;
    }

    hr = Device->CreateVertexShader(
        (DWORD*)shader->GetBufferPointer(),
        &ShaderVS);

    if (FAILED(hr))
    {
        ::MessageBox(0, "CreateVertexShader - FAILED", 0, 0);
        return false;
    }

    hr = D3DXCompileShaderFromFile(
        // path from build/msvc-.../bin
        "../../../src/min.ps",
        0,
        0,
        "Main",  // entry point function name
        "ps_2_0",// shader version to compile to
        D3DXSHADER_DEBUG | D3DXSHADER_USE_LEGACY_D3DX9_31_DLL,
        &shader,
        &errorBuffer,
        0);

    // output any error messages
    if (errorBuffer)
    {
        ::MessageBox(0, (char*)errorBuffer->GetBufferPointer(), 0, 0);
        d3d::Release<ID3DXBuffer*>(errorBuffer);
    }

    if (FAILED(hr))
    {
        ::MessageBox(0, "D3DXCreateEffectFromFile() - FAILED for PS", 0, 0);
        return false;
    }

    hr = Device->CreatePixelShader(
        (DWORD*)shader->GetBufferPointer(),
        &ShaderPS);

    if (FAILED(hr))
    {
        ::MessageBox(0, "CreatePixelShader - FAILED", 0, 0);
        return false;
    }

    d3d::Release<ID3DXBuffer*>(shader);

    return true;
}

void Cleanup()
{
    d3d::Release<IDirect3DVertexBuffer9*>(Triangle);
    d3d::Release<IDirect3DVertexShader9*>(ShaderVS);
    d3d::Release<IDirect3DPixelShader9*>(ShaderPS);
}

bool Display(float timeDelta)
{
    if (Device)
    {
        //
        // Render
        //

        Device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xffffffff, 1.0f, 0);
        Device->BeginScene();

        Device->SetStreamSource(0, Triangle, 0, sizeof(Vertex));
        Device->SetFVF(Vertex::FVF);

        
        Device->SetVertexShader(ShaderVS);
        Device->SetPixelShader(ShaderPS);
        Device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);

        Device->EndScene();
        Device->Present(0, 0, 0, 0);
    }
    return true;
}

//
// WndProc
//
LRESULT CALLBACK d3d::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch( msg )
	{
	case WM_DESTROY:
		::PostQuitMessage(0);
		break;
		
	case WM_KEYDOWN:
		if( wParam == VK_ESCAPE )
			::DestroyWindow(hwnd);
		break;
	}
	return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

//
// WinMain
//
int WINAPI WinMain(HINSTANCE hinstance,
				   HINSTANCE prevInstance, 
				   PSTR cmdLine,
				   int showCmd)
{
	if(!d3d::InitD3D(hinstance,
		Width, Height, true, D3DDEVTYPE_HAL, &Device))
	{
		::MessageBox(0, "InitD3D() - FAILED", 0, 0);
		return 0;
	}
		
	if(!Setup())
	{
		::MessageBox(0, "Setup() - FAILED", 0, 0);
		return 0;
	}

	d3d::EnterMsgLoop( Display );

	Cleanup();

	Device->Release();

	return 0;
}
