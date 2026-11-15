/* runtime/split: nvn_sdk_addrs.h for the runtime built apart from the player's tables (README.md). The sdk offsets of
 * runtime/gen/nvn_sdk_addrs.h are data in game.so (runtime/hl_tables.c), read at run time. */
#ifndef NVN_SDK_ADDRS_H
#define NVN_SDK_ADDRS_H
extern const unsigned long long hl_sdka_chip_descriptor;
extern const unsigned long long hl_sdka_format_count;
extern const unsigned long long hl_sdka_format_table;
extern const unsigned long long hl_sdka_nvnTextureBuilderGetRawStorageClass;
extern const unsigned long long hl_sdka_nvnTextureBuilderGetSparseTileLayout;
extern const unsigned long long hl_sdka_nvnTextureBuilderGetStorageAlignment;
extern const unsigned long long hl_sdka_nvnTextureBuilderGetStorageClass;
extern const unsigned long long hl_sdka_nvnTextureBuilderGetStorageSize;
extern const unsigned long long hl_sdka_nvnTextureGetRawStorageClass;
extern const unsigned long long hl_sdka_nvnTextureGetSparseTileLayout;
extern const unsigned long long hl_sdka_nvnTextureGetStorageClass;
extern const unsigned long long hl_sdka_nvnTextureGetStorageSize;
extern const unsigned long long hl_sdka_texture_layout;
extern const unsigned long long hl_sdka_vt_hal_gm200;
extern const unsigned long long hl_sdka_vt_nvgpu_device;
#define SDKA_chip_descriptor                      hl_sdka_chip_descriptor
#define SDKA_format_count                         hl_sdka_format_count
#define SDKA_format_table                         hl_sdka_format_table
#define SDKA_nvnTextureBuilderGetRawStorageClass  hl_sdka_nvnTextureBuilderGetRawStorageClass
#define SDKA_nvnTextureBuilderGetSparseTileLayout hl_sdka_nvnTextureBuilderGetSparseTileLayout
#define SDKA_nvnTextureBuilderGetStorageAlignment hl_sdka_nvnTextureBuilderGetStorageAlignment
#define SDKA_nvnTextureBuilderGetStorageClass     hl_sdka_nvnTextureBuilderGetStorageClass
#define SDKA_nvnTextureBuilderGetStorageSize      hl_sdka_nvnTextureBuilderGetStorageSize
#define SDKA_nvnTextureGetRawStorageClass         hl_sdka_nvnTextureGetRawStorageClass
#define SDKA_nvnTextureGetSparseTileLayout        hl_sdka_nvnTextureGetSparseTileLayout
#define SDKA_nvnTextureGetStorageClass            hl_sdka_nvnTextureGetStorageClass
#define SDKA_nvnTextureGetStorageSize             hl_sdka_nvnTextureGetStorageSize
#define SDKA_texture_layout                       hl_sdka_texture_layout
#define SDKA_vt_hal_gm200                         hl_sdka_vt_hal_gm200
#define SDKA_vt_nvgpu_device                      hl_sdka_vt_nvgpu_device
#endif
