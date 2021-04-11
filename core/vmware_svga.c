#include "vmware_svga.h"

#define PCI_VENDOR_ID_VMWARE            0x15AD
#define PCI_DEVICE_ID_VMWARE_SVGA2      0x0405

#define SVGA_INDEX_PORT         		0x0000
#define SVGA_VALUE_PORT         		0x0001
#define SVGA_BIOS_PORT          		0x0002
#define SVGA_IRQSTATUS_PORT     		0x0008

#define SVGA_REG_ENABLE					0x0001

static u32 ReadReg(u32 ioBase, u32 index)
{
	__outdword(ioBase + SVGA_INDEX_PORT, index);
	return __indword(ioBase + SVGA_VALUE_PORT);
}

// changes register in certain hardware
static void WriteReg(u32 ioBase, u32 index, u32 value)
{
	__outdword(ioBase + SVGA_INDEX_PORT, index);
	__outdword(ioBase + SVGA_VALUE_PORT, value);
}

// writes string in row and column
u8 VmwareSvgaPutString(PVmwareSvgaDevice device, char *s, u64 row, u64 column)
{
	if (device->isSvga)
		return VMWARE_SVGA_DEVICE_NOT_IN_VGA;
	char *p = (char*)phys_to_virt(0xB8000) + (row * 80 + column) * 2; // phys_to_virt := converts a physical address to virtual address, doesnt exist in bitvisor
	for (; *s; ++s, p += 2)
		*p = *s;
	return VMWARE_SVGA_OK;
}
// only for text mode
u8 VmwareSvgaColorFill(PVmwareSvgaDevice device, VgaColor back, VgaColor fore)
{
	if (device->isSvga)
		return VMWARE_SVGA_DEVICE_NOT_IN_VGA;
	char *p = (char*)phys_to_virt(0xB8000); // phys_to_virt := converts a physical address to virtual address, doesnt exist in bitvisor
	for (u64 i = 0; i < 80 * 25; ++i)
		p[i * 2 + 1] = (back << 4) | fore;
	return VMWARE_SVGA_OK;
}
// isSvga = false -> vga text
u8 VmwareSvgaSwitchTo(PVmwareSvgaDevice device, bool isSvga)
{
	device->isSvga = isSvga;
	WriteReg(device->ioAddress /*gpu address*/, SVGA_REG_ENABLE, isSvga /*false*/);
	return VMWARE_SVGA_OK;
}

u8 VmwareSvgaInit(PVmwareSvgaDevice device)
{
	device->pciAddress = PciFindDevice(PCI_VENDOR_ID_VMWARE, PCI_DEVICE_ID_VMWARE_SVGA2);
	if (device->pciAddress == -1u)
		return VMWARE_SVGA_DEVICE_NOT_FOUND;
	device->ioAddress = PciGetBar(device->pciAddress, 0); // gpu address
	device->isSvga = ReadReg(device->ioAddress, SVGA_REG_ENABLE); // true
	return VMWARE_SVGA_OK;
}
