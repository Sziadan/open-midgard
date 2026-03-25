#pragma once
#include "Types.h"
#include <windows.h>

// ============================================================================
// Miles Sound System (Mss32.dll)
// ============================================================================
typedef void (__stdcall *P_AIL_startup)();
typedef void (__stdcall *P_AIL_shutdown)();
typedef void (__stdcall *P_AIL_set_redist_directory)(const char *dir);
typedef void* (__stdcall *P_AIL_open_digital_driver)(u32 rate, u32 bits, u32 channels, u32 flags);
typedef void (__stdcall *P_AIL_close_digital_driver)(void *drvr);
typedef void* (__stdcall *P_AIL_allocate_sample_handle)(void *drvr);
typedef void (__stdcall *P_AIL_release_sample_handle)(void *sample);
typedef void (__stdcall *P_AIL_init_sample)(void *sample);
typedef void (__stdcall *P_AIL_set_sample_file)(void *sample, const void *file_image, s32 block);
typedef void (__stdcall *P_AIL_set_sample_volume)(void *sample, s32 volume);
typedef void (__stdcall *P_AIL_start_sample)(void *sample);
typedef void (__stdcall *P_AIL_stop_sample)(void *sample);
typedef u32 (__stdcall *P_AIL_sample_status)(void *sample);
typedef void (__stdcall *P_AIL_end_sample)(void *sample);
typedef void (__stdcall *P_AIL_pause_stream)(void *stream, s32 onoff);
typedef void* (__stdcall *P_AIL_open_stream)(void *drvr, const char *filename, u32 flags);
typedef void (__stdcall *P_AIL_close_stream)(void *stream);
typedef void (__stdcall *P_AIL_set_stream_volume)(void *stream, s32 volume);
typedef void (__stdcall *P_AIL_set_stream_loop_count)(void *stream, s32 count);
typedef void (__stdcall *P_AIL_start_stream)(void *stream);
typedef s32 (__stdcall *P_AIL_stream_volume)(void *stream);
typedef void (__stdcall *P_AIL_set_preference)(u32 number, s32 value);
typedef s32 (__stdcall *P_AIL_enumerate_3D_providers)(void *enum_state, void **handles, char **names);
typedef s32 (__stdcall *P_AIL_open_3D_provider)(void *handle);
typedef void (__stdcall *P_AIL_close_3D_provider)(void *handle);
typedef s32 (__stdcall *P_AIL_3D_speaker_type)(void *handle);
typedef void (__stdcall *P_AIL_set_3D_speaker_type)(void *handle, s32 type);
typedef void* (__stdcall *P_AIL_open_3D_listener)(void *handle);
typedef void (__stdcall *P_AIL_close_3D_listener)(void *listener);
typedef void* (__stdcall *P_AIL_allocate_3D_sample_handle)(void *handle);
typedef void (__stdcall *P_AIL_release_3D_sample_handle)(void *sample);
typedef u32 (__stdcall *P_AIL_3D_sample_status)(void *sample);
typedef s32 (__stdcall *P_AIL_set_3D_sample_file)(void *sample, const void *file_image);
typedef void (__stdcall *P_AIL_set_3D_sample_effects_level)(void *sample, float level);
typedef void (__stdcall *P_AIL_set_3D_position)(void *sample, float x, float y, float z);
typedef void (__stdcall *P_AIL_set_3D_orientation)(void *sample, float fx, float fy, float fz, float ux, float uy, float uz);
typedef void (__stdcall *P_AIL_set_3D_sample_volume)(void *sample, s32 volume);
typedef void (__stdcall *P_AIL_set_3D_sample_distances)(void *sample, float max_dist, float min_dist);
typedef void (__stdcall *P_AIL_start_3D_sample)(void *sample);
typedef void (__stdcall *P_AIL_end_3D_sample)(void *sample);
typedef s32 (__stdcall *P_AIL_3D_room_type)(void *handle);
typedef void (__stdcall *P_AIL_set_3D_room_type)(void *handle, s32 type);

// ============================================================================
// Bink Video (binkw32.dll)
// ============================================================================
typedef void* (__stdcall *P_BinkOpen)(const char *name, u32 flags);
typedef void (__stdcall *P_BinkClose)(void *bink);
typedef s32 (__stdcall *P_BinkSetSoundSystem)(void *open_ptr, void *open_flags);
typedef void* (__stdcall *P_BinkOpenDirectSound)(u32 flags);
typedef s32 (__stdcall *P_BinkDoFrame)(void *bink);
typedef void (__stdcall *P_BinkCopyToBuffer)(void *bink, void *dest, u32 pitch, u32 height, u32 x, u32 y, u32 buf_type);
typedef void (__stdcall *P_BinkNextFrame)(void *bink);
typedef s32 (__stdcall *P_BinkWait)(void *bink);
typedef s32 (__stdcall *P_BinkPause)(void *bink, s32 pause);
typedef void (__stdcall *P_BinkGoto)(void *bink, u32 frame, u32 flags);
typedef s32 (__stdcall *P_BinkDDSurfaceType)(void *surface);

// ============================================================================
// Intel JPEG Library (ijl15.dll)
// ============================================================================
typedef s32 (__stdcall *P_ijlInit)(void *props);
typedef s32 (__stdcall *P_ijlRead)(void *props, u32 io_type);
typedef s32 (__stdcall *P_ijlFree)(void *props);

// ============================================================================
// Granny 3D (granny2.dll)
// ============================================================================
typedef void* (__stdcall *P_GrannyReadEntireFileFromMemory)(u32 size, const void *mem);
typedef void* (__stdcall *P_GrannyGetFileInfo)(void *file);
typedef void (__stdcall *P_GrannyFreeFile)(void *file);
typedef void* (__stdcall *P_GrannyInstantiateModel)(void *model);
typedef void* (__stdcall *P_GrannyNewWorldPose)(s32 bone_count);
typedef float* (__stdcall *P_GrannyGetWorldPoseComposite4x4Array)(void *pose);
typedef s32* (__stdcall *P_GrannyGetMeshBindingToBoneIndices)(void *binding);
typedef s32 (__stdcall *P_GrannyGetMeshVertexCount)(void *mesh);
typedef u32* (__stdcall *P_GrannyGetMeshTriangleGroups)(void *mesh);
typedef u8 (__stdcall *P_GrannyMeshIsRigid)(void *mesh);
typedef float* (__stdcall *P_GrannyGetWorldPoseComposite4x4)(void *pose, s32 bone_idx);
typedef s32 (__stdcall *P_GrannyGetMeshIndexCount)(void *mesh);
typedef void (__stdcall *P_GrannyDeformVertices)(void *deformer, const s32 *bone_indices, const float *composite4x4, s32 vertex_count, void *source_vertices, void *dest_vertices);
typedef void* (__stdcall *P_GrannyGetMeshVertices)(void *mesh);
typedef s32 (__stdcall *P_GrannyGetMeshBytesPerIndex)(void *mesh);
typedef void* (__stdcall *P_GrannyGetMeshIndices)(void *mesh);
typedef void* (__stdcall *P_GrannyGetSourceSkeleton)(void *model_instance);
typedef void* (__stdcall *P_GrannyNewMeshBinding)(void *mesh, void *source_skeleton, void *dest_skeleton);
typedef void (__stdcall *P_GrannyCopyMeshIndices)(void *mesh, s32 bytes_per_index, void *dest);
typedef void* (__stdcall *P_GrannyGetMeshVertexType)(void *mesh);
typedef void* (__stdcall *P_GrannyNewMeshDeformer)(void *source_vertex_type, void *dest_vertex_type, s32 animation_type);
typedef void (__stdcall *P_GrannyCopyMeshVertices)(void *mesh, void *vertex_type, void *dest);
typedef u8 (__stdcall *P_GrannyTextureHasAlpha)(void *texture);
typedef void (__stdcall *P_GrannyCopyTextureImage)(void *texture, s32 mip_level, s32 image_index, void *pixel_format, s32 width, s32 height, s32 pitch, void *dest);
typedef void* (__stdcall *P_GrannyGetMaterialTextureByType)(void *material, s32 type);

// ============================================================================
// CPS Compression (cps.dll)
// ============================================================================
// zlib's uncompress uses cdecl on Win32; using stdcall causes ESP mismatch.
typedef s32 (__cdecl *P_uncompress)(u8 *dest, u32 *destLen, const u8 *source, u32 sourceLen);

// ============================================================================
// Debug Help (dbghelp.dll)
// ============================================================================
typedef BOOL (__stdcall *P_SymInitialize)(HANDLE hProcess, const char* UserSearchPath, BOOL fInvadeProcess);
typedef BOOL (__stdcall *P_MiniDumpWriteDump)(HANDLE hProcess, DWORD ProcessId, HANDLE hFile, int DumpType, void* ExceptionParam, void* UserStreamParam, void* CallbackParam);

struct DLL_EXPORTS {
    // Miles
    P_AIL_startup AIL_startup;
    P_AIL_shutdown AIL_shutdown;
    P_AIL_set_redist_directory AIL_set_redist_directory;
    P_AIL_open_digital_driver AIL_open_digital_driver;
    P_AIL_close_digital_driver AIL_close_digital_driver;
    P_AIL_allocate_sample_handle AIL_allocate_sample_handle;
    P_AIL_release_sample_handle AIL_release_sample_handle;
    P_AIL_init_sample AIL_init_sample;
    P_AIL_set_sample_file AIL_set_sample_file;
    P_AIL_set_sample_volume AIL_set_sample_volume;
    P_AIL_start_sample AIL_start_sample;
    P_AIL_stop_sample AIL_stop_sample;
    P_AIL_sample_status AIL_sample_status;
    P_AIL_end_sample AIL_end_sample;
    P_AIL_pause_stream AIL_pause_stream;
    P_AIL_open_stream AIL_open_stream;
    P_AIL_close_stream AIL_close_stream;
    P_AIL_set_stream_volume AIL_set_stream_volume;
    P_AIL_set_stream_loop_count AIL_set_stream_loop_count;
    P_AIL_start_stream AIL_start_stream;
    P_AIL_stream_volume AIL_stream_volume;
    P_AIL_set_preference AIL_set_preference;
    P_AIL_enumerate_3D_providers AIL_enumerate_3D_providers;
    P_AIL_open_3D_provider AIL_open_3D_provider;
    P_AIL_close_3D_provider AIL_close_3D_provider;
    P_AIL_3D_speaker_type AIL_3D_speaker_type;
    P_AIL_set_3D_speaker_type AIL_set_3D_speaker_type;
    P_AIL_open_3D_listener AIL_open_3D_listener;
    P_AIL_close_3D_listener AIL_close_3D_listener;
    P_AIL_allocate_3D_sample_handle AIL_allocate_3D_sample_handle;
    P_AIL_release_3D_sample_handle AIL_release_3D_sample_handle;
    P_AIL_3D_sample_status AIL_3D_sample_status;
    P_AIL_set_3D_sample_file AIL_set_3D_sample_file;
    P_AIL_set_3D_sample_effects_level AIL_set_3D_sample_effects_level;
    P_AIL_set_3D_position AIL_set_3D_position;
    P_AIL_set_3D_orientation AIL_set_3D_orientation;
    P_AIL_set_3D_sample_volume AIL_set_3D_sample_volume;
    P_AIL_set_3D_sample_distances AIL_set_3D_sample_distances;
    P_AIL_start_3D_sample AIL_start_3D_sample;
    P_AIL_end_3D_sample AIL_end_3D_sample;
    P_AIL_3D_room_type AIL_3D_room_type;
    P_AIL_set_3D_room_type AIL_set_3D_room_type;

    // Bink
    P_BinkOpen BinkOpen;
    P_BinkClose BinkClose;
    P_BinkSetSoundSystem BinkSetSoundSystem;
    P_BinkOpenDirectSound BinkOpenDirectSound;
    P_BinkDoFrame BinkDoFrame;
    P_BinkCopyToBuffer BinkCopyToBuffer;
    P_BinkNextFrame BinkNextFrame;
    P_BinkWait BinkWait;
    P_BinkPause BinkPause;
    P_BinkGoto BinkGoto;
    P_BinkDDSurfaceType BinkDDSurfaceType;

    // IJL
    P_ijlInit ijlInit;
    P_ijlRead ijlRead;
    P_ijlFree ijlFree;

    // Granny
    P_GrannyReadEntireFileFromMemory GrannyReadEntireFileFromMemory;
    P_GrannyGetFileInfo GrannyGetFileInfo;
    P_GrannyFreeFile GrannyFreeFile;
    P_GrannyInstantiateModel GrannyInstantiateModel;
    P_GrannyNewWorldPose GrannyNewWorldPose;
    P_GrannyGetWorldPoseComposite4x4Array GrannyGetWorldPoseComposite4x4Array;
    P_GrannyGetMeshBindingToBoneIndices GrannyGetMeshBindingToBoneIndices;
    P_GrannyGetMeshVertexCount GrannyGetMeshVertexCount;
    P_GrannyGetMeshTriangleGroups GrannyGetMeshTriangleGroups;
    P_GrannyMeshIsRigid GrannyMeshIsRigid;
    P_GrannyGetWorldPoseComposite4x4 GrannyGetWorldPoseComposite4x4;
    P_GrannyGetMeshIndexCount GrannyGetMeshIndexCount;
    P_GrannyDeformVertices GrannyDeformVertices;
    P_GrannyGetMeshVertices GrannyGetMeshVertices;
    P_GrannyGetMeshBytesPerIndex GrannyGetMeshBytesPerIndex;
    P_GrannyGetMeshIndices GrannyGetMeshIndices;
    P_GrannyGetSourceSkeleton GrannyGetSourceSkeleton;
    P_GrannyNewMeshBinding GrannyNewMeshBinding;
    P_GrannyCopyMeshIndices GrannyCopyMeshIndices;
    P_GrannyGetMeshVertexType GrannyGetMeshVertexType;
    P_GrannyNewMeshDeformer GrannyNewMeshDeformer;
    P_GrannyCopyMeshVertices GrannyCopyMeshVertices;
    P_GrannyTextureHasAlpha GrannyTextureHasAlpha;
    P_GrannyCopyTextureImage GrannyCopyTextureImage;
    P_GrannyGetMaterialTextureByType GrannyGetMaterialTextureByType;

    // CPS
    P_uncompress uncompress;

    // DbgHelp
    P_SymInitialize SymInitialize;
    P_MiniDumpWriteDump MiniDumpWriteDump;
};

extern DLL_EXPORTS g_dllExports;
