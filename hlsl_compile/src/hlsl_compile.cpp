
#ifdef _WIN32
#include <d3dcompiler.h>
#else
#define NOMINMAX
#include "vkd3d_hlsl.h"
#endif // _WIN32

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

// Additional functions

bool LoadFile(const char* file_name, char** ppBuffer, uint32_t* dwSize)
{
	if (ppBuffer == nullptr)
		return false;

	std::ifstream fileS(file_name, std::ios::binary);
	if (!fileS.is_open())
	{
		std::cout << "Can't open file" << "\n";
		return false;
	}
	const auto dwLowSize = std::filesystem::file_size(file_name);
	if (dwSize)
	{
		*dwSize = dwLowSize;
	}
	if (dwLowSize == 0)
	{
		*ppBuffer = nullptr;
		return false;
	}

	*ppBuffer = new char[dwLowSize];
	fileS.exceptions(std::fstream::failbit | std::fstream::badbit);
	fileS.read(*ppBuffer, dwLowSize);
	fileS.close();
	return true;
}

// Framework Functions

bool Setup()
{
	// Vertex shader

	HRESULT hr = 0;
	ID3DBlob* shader;
	ID3DBlob* errorMsg;

	char* data = nullptr;
	uint32_t size = 0;
	if (LoadFile("min.vs", &data, &size) == false || !data)
	{
		std::cout << "Can't load VS file" << "\n";
		return false;
	}

	hr = D3DCompile(data, size, nullptr, nullptr, nullptr, "Main", "vs_1_1", 0, 0, &shader, &errorMsg);
	if (errorMsg)
	{
		std::cout << (char*)errorMsg->GetBufferPointer() << "\n";
	}

	if (FAILED(hr))
	{
		std::cout << "D3DCompile() - FAILED for VS" << "\n";
		return false;
	}

	// Pixel shader

	delete[] data;
	data = nullptr;
	size = 0;
	if (LoadFile("min.ps", &data, &size) == false || !data)
	{
		std::cout << "Can't load PS file" << "\n";
		return false;
	}

	hr = D3DCompile(data, size, nullptr, nullptr, nullptr, "Main", "ps_2_0", 0, 0, &shader, &errorMsg);
	if (errorMsg)
	{
		std::cout << (char*)errorMsg->GetBufferPointer() << "\n";
	}

	if (FAILED(hr))
	{
		std::cout << "D3DCompile() - FAILED for PS" << "\n";
		return false;
	}

	delete[] data;

	return true;
}

int main(int argc, char* argv[]) {

	if (!Setup())
	{
		std::cout << "Setup() - FAILED" << "\n";
		return 0;
	}

	std::cout << "Setup() - SUCCESS" << "\n";
	return 0;
}
