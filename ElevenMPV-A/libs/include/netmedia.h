/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 04.508.001
* Copyright (C) 2015 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _NETMEDIA_H
#define _NETMEDIA_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NM_PRX_BUILD
#define PRX_EXPORT __declspec(dllexport)
#else
#define PRX_EXPORT
#endif

/// @brief
/// The NETMedia library handle definition.
///
/// @ingroup libNetMedia
	typedef void* SceNmHandle;

/// @brief
/// Initialises the NETMedia library.
///
/// @param nmHandle			[O] Pointer to receive the handle of the initialised NETMedia library.
/// @param pFileReplacement	[O] Pointer to a file replacement structure that will receive the functions to be used
///								for the NETMedia library. These should be passed in the call to sceAvPlayerInit.
/// @param pNetBuffer		[I] User allocated buffer to use as the download buffer.  If NULL then a buffer of size
///								buffsz will be allocated internally (the allocation will use pMemAllocator if set).
/// @param buffsz			[I] Size of the user allocated buffer or the size to allocate internally.
/// @param sslCtxId			[I] Identifier returned when the SSL stack was initialised.
/// @param httpCtxId		[I] Context received when the underlying HTTP stack was initialised.
/// @param pMemAllocator	[I] Pointer to the memory allocator instance to be used for all allocations.
///								Optional parameter that may be set to NULL (default OS allocators will be used).
/// @retval
/// 0						The NETMedia library was initialised successfully.
/// @retval
/// <0						Initialisation failed.
///
/// @ingroup libNetMedia
PRX_EXPORT int32_t NETMediaInit(SceNmHandle* nmHandle,
					 SceAvPlayerFileReplacement* pFileReplacement,
					 uint8_t* pNetBuffer, 
					 size_t buffsz, 
					 int32_t sslCtxId, 
					 int32_t httpCtxId, 
					 SceAvPlayerMemAllocator* pMemAllocator = NULL);

/// @brief
/// Terminates the NETMedia library.
///
/// @param nmHandle			[I] The NETMedia library handle as returned by NETMediaInit.
/// @param pMemAllocator	[I] Pointer to the memory allocator instance passed in to NETMediaInit.
//								This must match what was passed in to NETMediaInit.
/// @retval
/// 0						The NETMedia library was terminated successfully.
/// @retval
/// <0						Termination failed. The return value will contain an error from SceLsmResult.
///
/// @ingroup libNetMedia
PRX_EXPORT int32_t NETMediaDeInit(SceNmHandle nmHandle, SceAvPlayerMemAllocator* pMemAllocator = NULL);

/// @brief
/// Invalidates all NETMedia buffers.
///
/// @param nmHandle			[I] The NETMedia library handle as returned by NETMediaInit.
///
/// @retval
/// 0						Buffers invalidated.
/// @retval
/// <0						Invalidation failed.
///
/// @ingroup libNetMedia
PRX_EXPORT int32_t NETMediaInvalidateAllBuffers(SceNmHandle nmHandle);


#ifdef __cplusplus
}
#endif

#endif  //_NETMEDIA_H
