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
	  ../picodrive/pico/sound/mix.o \
	  ../picodrive/cpu/fame/famec.o \
	  ../picodrive/cpu/cz80/cz80.o \
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
	  ../picodrive/zlib/zutil.o

# Additional C compiler flags for Picodrive core
CFLAGS   += -I../picodrive -I../picodrive/pico -I../picodrive/cpu/fame -I../picodrive/cpu/cz80 -I../picodrive/zlib -I../picodrive/pico/sound -DEMU_F68K -D_USE_CZ80 -DNO_32X -DNO_SMS -O3 -Wno-unused-result -Wno-write-strings -falign-functions=2
CPPFLAGS += -I../picodrive -I../picodrive/pico -I../picodrive/cpu/fame -I../picodrive/cpu/cz80 -I../picodrive/zlib -I../picodrive/pico/sound -DEMU_F68K -D_USE_CZ80 -DNO_32X -DNO_SMS -O3 -falign-functions=2

LIBS	= $(CIRCLEHOME)/lib/sound/libsound.a \
	  $(CIRCLEHOME)/lib/fs/fat/libfatfs.a \
	  $(CIRCLEHOME)/lib/fs/libfs.a \
	  $(CIRCLEHOME)/lib/input/libinput.a \
	  $(CIRCLEHOME)/lib/usb/libusb.a \
	  $(CIRCLEHOME)/lib/sched/libsched.a \
	  $(CIRCLEHOME)/lib/libcircle.a

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
