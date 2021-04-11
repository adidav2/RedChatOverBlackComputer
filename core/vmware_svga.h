#include <types.h>
#include "mm.h"

#define VMWARE_SVGA_OK						0
#define VMWARE_SVGA_DEVICE_NOT_FOUND 		1
#define VMWARE_SVGA_DEVICE_NOT_IN_VGA 		2

typedef enum
{
	VGA_COLOR_BLACK,
	VGA_COLOR_BLUE,
	VGA_COLOR_GREEN,
	VGA_COLOR_CYAN,
	VGA_COLOR_RED,
	VGA_COLOR_MAGENTA,
	VGA_COLOR_BROWN,
	VGA_COLOR_LIGHT_GRAY,
	VGA_COLOR_DARK_GRAY,
	VGA_COLOR_LIGHT_BLUE,
	VGA_COLOR_LIGHT_GREEN,
	VGA_COLOR_LIGHT_CYAN,
	VGA_COLOR_LIGHT_RED,
	VGA_COLOR_LIGHT_MAGENTA,
	VGA_COLOR_YELLOW,
	VGA_COLOR_WHITE,
} VgaColor;

typedef struct
{
	u32 pciAddress;
	u32 ioAddress;
	bool isSvga;
} VmwareSvgaDevice, *PVmwareSvgaDevice;

u8 VmwareSvgaInit(PVmwareSvgaDevice device);
u8 VmwareSvgaSwitchTo(PVmwareSvgaDevice device, bool isSvga);
u8 VmwareSvgaPutString(PVmwareSvgaDevice device, char *s, u64 row, u64 column);
u8 VmwareSvgaColorFill(PVmwareSvgaDevice device, VgaColor back, VgaColor fore);
