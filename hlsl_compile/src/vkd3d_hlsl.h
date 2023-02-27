
#include "windows.h"
#include <vkd3d_types.h>

//
////////////////////////////// vkd3d_d3dcommon.h //////////////////////////////
//

#ifndef __ID3D10Blob_INTERFACE_DEFINED__
#define __ID3D10Blob_INTERFACE_DEFINED__

DEFINE_GUID(IID_ID3D10Blob, 0x8ba5fb08, 0x5195, 0x40e2, 0xac,0x58, 0x0d,0x98,0x9c,0x3a,0x01,0x02);
#if defined(__cplusplus) && !defined(CINTERFACE)
MIDL_INTERFACE("8ba5fb08-5195-40e2-ac58-0d989c3a0102")
ID3D10Blob : public IUnknown
{
	virtual void * STDMETHODCALLTYPE GetBufferPointer(
		) = 0;

	virtual SIZE_T STDMETHODCALLTYPE GetBufferSize(
		) = 0;

};
__CRT_UUID_DECL(ID3D10Blob, 0x8ba5fb08, 0x5195, 0x40e2, 0xac,0x58, 0x0d,0x98,0x9c,0x3a,0x01,0x02)
#endif
#endif

typedef ID3D10Blob ID3DBlob;
typedef enum _D3D_INCLUDE_TYPE {
	D3D_INCLUDE_LOCAL = 0,
	D3D_INCLUDE_SYSTEM = 1,
	D3D10_INCLUDE_LOCAL = D3D_INCLUDE_LOCAL,
	D3D10_INCLUDE_SYSTEM = D3D_INCLUDE_SYSTEM,
	D3D_INCLUDE_FORCE_DWORD = 0x7fffffff
} D3D_INCLUDE_TYPE;
/*****************************************************************************
 * ID3DInclude interface
 */
#ifndef __ID3DInclude_INTERFACE_DEFINED__
#define __ID3DInclude_INTERFACE_DEFINED__

#if defined(__cplusplus) && !defined(CINTERFACE)
interface ID3DInclude
{

	BEGIN_INTERFACE

		virtual HRESULT STDMETHODCALLTYPE Open(
			D3D_INCLUDE_TYPE include_type,
			const char *filename,
			const void *parent_data,
			const void **data,
			UINT *size) = 0;

	virtual HRESULT STDMETHODCALLTYPE Close(
		const void *data) = 0;

	END_INTERFACE

};
#endif
#endif

typedef struct _D3D_SHADER_MACRO {
	const char *Name;
	const char *Definition;
} D3D_SHADER_MACRO;


//
////////////////////////////// vkd3d_utils.h //////////////////////////////
//

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

# define VKD3D_UTILS_API VKD3D_IMPORT

VKD3D_UTILS_API HRESULT WINAPI D3DCompile(const void *data, SIZE_T data_size, const char *filename,
										  const D3D_SHADER_MACRO *defines, ID3DInclude *include, const char *entrypoint,
										  const char *target, UINT flags, UINT effect_flags, ID3DBlob **shader, ID3DBlob **error_messages);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
