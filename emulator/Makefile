#
# Makefile
#

CIRCLEHOME = ../circle

OBJS	= main.o kernel.o osd.o emu_orchestrator.o clib_stubs.o \
	  ../picodrive/pico/pico.o \
	  ../picodrive/pico/cart.o \
	  ../picodrive/pico/memory.o \
	  ../picodrive/pico/state.o \
	  ../picodrive/pico/sek.o \
	  ../picodrive/pico/z80if.o \
	  ../picodrive/pico/videoport.o \
	  ../picodrive/pico/draw2.o \
	  ../picodrive/pico/draw.o \
	  ../picodrive/pico/mode4.o \
	  ../picodrive/pico/misc.o \
	  ../picodrive/pico/eeprom.o \
	  ../picodrive/pico/patch.o \
	  ../picodrive/pico/debug.o \
	  ../picodrive/pico/media.o \
	  ../picodrive/pico/carthw/carthw.o \
	  ../picodrive/pico/carthw/eeprom_spi.o \
	  ../picodrive/pico/carthw/svp/svp.o \
	  ../picodrive/pico/carthw/svp/memory.o \
	  ../picodrive/pico/carthw/svp/ssp16.o \
	  ../picodrive/pico/sound/sound.o \
	  ../picodrive/pico/sound/resampler.o \
	  ../picodrive/pico/sound/sn76496.o \
	  ../picodrive/pico/sound/ym2612.o \
	  ../picodrive/pico/sound/ym2413.o \
	  ../picodrive/pico/sound/vgm.o \
	  ../picodrive/pico/m68kif_cyclone.o \
	  ../picodrive/cpu/cyclone/Cyclone.o \
	  ../picodrive/cpu/cyclone/tools/idle.o \
	  ../picodrive/cpu/DrZ80/drz80.o \
	  ../picodrive/cpu/drc/cmn.o \
	  ../picodrive/pico/draw_arm.o \
	  ../picodrive/pico/draw2_arm.o \
	  ../picodrive/pico/memory_arm.o \
	  ../picodrive/pico/sound/ym2612_arm.o \
	  ../picodrive/pico/sound/mix_arm.o \
	  ../picodrive/pico/misc_arm.o \
	  ../picodrive/pico/cd/memory_arm.o \
	  ../picodrive/pico/cd/misc_arm.o \
	  ../picodrive/pico/cd/mcd.o \
	  ../picodrive/pico/cd/memory.o \
	  ../picodrive/pico/cd/sek.o \
	  ../picodrive/pico/cd/cdc.o \
	  ../picodrive/pico/cd/cdd.o \
	  ../picodrive/pico/cd/cd_image.o \
	  ../picodrive/pico/cd/cd_parse.o \
	  ../picodrive/pico/cd/gfx.o \
	  ../picodrive/pico/cd/gfx_dma.o \
	  ../picodrive/pico/cd/misc.o \
	  ../picodrive/pico/cd/pcm.o \
	  ../picodrive/pico/cd/megasd.o \
	  ../picodrive/pico/pico/pico.o \
	  ../picodrive/pico/pico/memory.o \
	  ../picodrive/pico/pico/xpcm.o \
	  ../picodrive/zlib/adler32.o \
	  ../picodrive/zlib/compress.o \
	  ../picodrive/zlib/crc32.o \
	  ../picodrive/zlib/deflate.o \
	  ../picodrive/zlib/gzio.o \
	  ../picodrive/zlib/infback.o \
	  ../picodrive/zlib/inffast.o \
	  ../picodrive/zlib/inflate.o \
	  ../picodrive/zlib/inftrees.o \
	  ../picodrive/zlib/trees.o \
	  ../picodrive/zlib/uncompr.o \
	  ../picodrive/zlib/zutil.o \
	  ../picodrive/pico/cd/libchdr/src/libchdr_chd.o \
	  ../picodrive/pico/cd/libchdr/src/libchdr_cdrom.o \
	  ../picodrive/pico/cd/libchdr/src/libchdr_flac.o \
	  ../picodrive/pico/cd/libchdr/src/libchdr_bitstream.o \
	  ../picodrive/pico/cd/libchdr/src/libchdr_huffman.o \
	  ../picodrive/pico/cd/libchdr/deps/lzma-24.05/src/CpuArch.o \
	  ../picodrive/pico/cd/libchdr/deps/lzma-24.05/src/Alloc.o \
	  ../picodrive/pico/cd/libchdr/deps/lzma-24.05/src/LzmaEnc.o \
	  ../picodrive/pico/cd/libchdr/deps/lzma-24.05/src/Sort.o \
	  ../picodrive/pico/cd/libchdr/deps/lzma-24.05/src/LzmaDec.o \
	  ../picodrive/pico/cd/libchdr/deps/lzma-24.05/src/LzFind.o \
	  ../picodrive/pico/cd/libchdr/deps/lzma-24.05/src/Delta.o \
	  ../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/common/entropy_common.o \
	  ../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/common/error_private.o \
	  ../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/common/fse_decompress.o \
	  ../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/common/xxhash.o \
	  ../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/common/zstd_common.o \
	  ../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/decompress/huf_decompress.o \
	  ../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/decompress/zstd_ddict.o \
	  ../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/decompress/zstd_decompress_block.o \
	  ../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib/decompress/zstd_decompress.o

# Get version from root VERSION file
MEGAPI_VERSION = $(shell cat ../VERSION)

# Additional C compiler flags for Picodrive core
DEFS     = -D_USE_DRZ80 -DEMU_C68K -D_ASM_DRAW_C -D_ASM_MEMORY_C -D_ASM_YM2612_C -D_ASM_MISC_C -D_ASM_CD_MEMORY_C -DNO_32X -DNO_SMS -DUSE_LIBCHDR -DZ7_ST -DEL_LOGMASK=0xc0000000 -DMEGAPI_VERSION="\"$(MEGAPI_VERSION)\""
INCS     = -I../picodrive -I../picodrive/pico -I../picodrive/cpu -I../picodrive/pico/sound -I../picodrive/zlib -I$(CIRCLEHOME)/addon/fatfs -I$(CIRCLEHOME)/addon \
           -I../picodrive/pico/cd/libchdr/include \
           -I../picodrive/pico/cd/libchdr/deps/lzma-24.05/include \
           -I../picodrive/pico/cd/libchdr/deps/zstd-1.5.6/lib

OPTIMIZE = -O3 -ffast-math -fomit-frame-pointer -ftree-vectorize -funroll-loops

CFLAGS   += $(INCS) $(DEFS) -Wno-unused-result -Wno-write-strings -falign-functions=2
CPPFLAGS += $(INCS) $(DEFS) -falign-functions=2
AFLAGS   += $(INCS) $(DEFS)

LIBS	= $(CIRCLEHOME)/addon/fatfs/libfatfs.a \
	  $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
	  $(CIRCLEHOME)/lib/sound/libsound.a \
	  $(CIRCLEHOME)/lib/fs/libfs.a \
	  $(CIRCLEHOME)/lib/input/libinput.a \
	  $(CIRCLEHOME)/lib/usb/libusb.a \
	  $(CIRCLEHOME)/lib/sched/libsched.a \
	  $(CIRCLEHOME)/lib/libcircle.a

include $(CIRCLEHOME)/Rules.mk

# Custom assembler rule for lowercase .s files (Cyclone, etc.)
%.o: %.s
	@echo "  AS    $@"
	@$(AS) $(AFLAGS) -c -o $@ $<

# Generate ILP32 offset header for ASM files dynamically
../picodrive/pico/pico_int_offs.h:
	cd ../picodrive/tools && CC="arm-none-eabi-gcc" CFLAGS="$(DEFS) -I../pico/sound" sh ./mkoffsets.sh ../pico

# Make all objects depend on generated offset header
$(OBJS): ../picodrive/pico/pico_int_offs.h

# Rebuild kernel.o when VERSION changes
kernel.o: ../VERSION

-include $(DEPS)
