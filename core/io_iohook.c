/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Tsukuba nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "current.h"
#include "debug.h"
#include "initfunc.h"
#include "io_io.h"
#include "io_iohook.h"
#include "panic.h"
#include "printf.h"

#include <stdlib.h>
#include "vmware_svga.h"

static bool use_shift_flag = false;
static bool ctrl_down_flag = false;
static bool capture_keyboard_flag = false;
static bool to_init_svga = true;
static PVmwareSvgaDevice Pdevice;

static const int scancode_to_asciichar[128] = {
	-1, '\e', '1', '2', '3', '4', '5', '6',
	'7', '8', '9', '0', '-', '=', '\b', '\t',
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
	'o', 'p', '[', ']', '\n', -1, 'a', 's',
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
	'\'', '`', -1, '\\', 'z', 'x', 'c', 'v',
	'b', 'n', 'm', ',', '.', '/', -1, '*',
	-1, ' ', -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1 /*(1)^A*/,
	-1 /*(16)^P*/, -1, '-', -1 /*(2)^B*/, -1, -1 /*(6)^F*/, '+', -1 /*(5)^E*/,
	-1 /*(14)^N*/, -1 /*(22)^V*/, -1, -1 /*(4)^D*/, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, '\\', -1, -1,
};
static const int scancode_to_asciichar_shift[128] = {
	-1, '\e', '!', '@', '#', '$', '%', '^',
	'&', '*', '(', ')', '_', '+', '\b', '\t',
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
	'O', 'P', '{', '}', '\n', -1, 'A', 'S',
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
	'"', '~', -1, '|', 'Z', 'X', 'C', 'V',
	'B', 'N', 'M', '<', '>', '?', -1, '*',
	-1, ' ', -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1 /*(1)^A*/,
	-1 /*(16)^P*/, -1, '-', -1 /*(2)^B*/, -1, -1 /*(6)^F*/, '+', -1 /*(5)^E*/,
	-1 /*(14)^N*/, -1 /*(22)^V*/, -1, -1 /*(4)^D*/, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, '|', -1, -1,
};


#ifdef DEBUG_IO_MONITOR
static enum ioact
kbdio_monitor (enum iotype type, u32 port, void *data)
{
	do_io_default (type, port, data);
	if (type == IOTYPE_INB)
		printf ("IO Monitor test: INB PORT=0x60 DATA=0x%X\n", *(u8 *)data);
	return IOACT_CONT;
}
#endif

#ifdef DEBUG_IO0x20_MONITOR
#include "vramwrite.h"
static enum ioact
io0x20_monitor (enum iotype type, u32 port, void *data)
{
	do_io_default (type, port, data);
	if (type == IOTYPE_OUTB) {
		vramwrite_save_and_move_cursor (56, 23);
		printf ("OUT0x20,0x%02X", *(u8 *)data);
		vramwrite_restore_cursor ();
	}
	return IOACT_CONT;
}
#endif

#if defined (F11PANIC) || defined (F12MSG)
#include "keyboard.h"
static enum ioact
kbdio_dbg_monitor (enum iotype type, u32 port, void *data)
{
	static int led = 0;
	static u8 lk = 0;
#ifdef NTTCOM_TEST
        unsigned long int session;
        int i, j;
        char sig[1024];
        unsigned short int siglen ;
#endif

	do_io_default (type, port, data);
	if (type == IOTYPE_INB) {
#ifdef CARDSTATUS
		extern int ps2_locked;
		if (ps2_locked) {
			printf ("Ignoring PS/2 input\n");
			*(u8 *)data = 0;
			return IOACT_CONT;
		}
#endif
		switch (*(u8 *)data) {
#if defined(F10USBTEST)
		case 0x44 | 0x80: /* F10 */
			if (lk == 0x44) {
				extern void usb_api_batchtest(void);
			
				printf ("F10 pressed.\n");
				usb_api_batchtest();
			}
			break;
#endif /* defined(F10USBTEST) */
		case 0x57 | 0x80: /* F11 */
			if (lk == 0x57) {
#ifdef NTTCOM_TEST
                                printf ("F11 pressed.\n");
                                printf ("IDMan_IPInitializeReader.\n");
                                i = IDMan_IPInitializeReader( );
                                printf ("IDMan_IPInitializeReader return = %d.\n", i);
                                printf ("IDMan_IPInitialize.\n");
                                i = IDMan_IPInitialize("123456789@ABCDEF",  &session);
                                printf ("IDMan_IPInitialize return = %d.\n", i);
                                printf ("IDMan_generateSignatureByIndex.\n");
                                i = IDMan_generateSignatureByIndex( session, 1, "1234567890abcdef", strlen("1234567890abcdef"), sig, &siglen, 544);
                                printf ("IDMan_generateSignatureByIndex return = %d siglen=%d\n", i, siglen);
                                printf ("IDMan_IPFinalize.\n");
                                i = IDMan_IPFinalize(session);
                                printf ("IDMan_IPFinalize return = %d.\n", i);
                                printf ("IDMan_IPFinalizeReader.\n");
                                i = IDMan_IPFinalizeReader( );
                                printf ("IDMan_IPFinalizeReader return = %d.\n", i);
#else
#ifdef F11PANIC
				if (config.vmm.f11panic)
					panic ("F11 pressed.");
#endif
#endif
			}
			break;
		case 0x58 | 0x80: /* F12 */
			if (lk == 0x58) {
#if defined(F12DUMPEHCI)
				extern void ehci_dump_all(int, void *);
#endif /* defined(F12DUMPEHCI) */
#if defined(F12MSG)
				if (config.vmm.f12msg) {
					debug_gdb ();
					led ^= LED_CAPSLOCK_BIT |
						LED_NUMLOCK_BIT;
					setkbdled (led);
					printf ("F12 pressed.\n");
				}
#endif /* defined(F12MSG) */
#if defined(F12DUMPEHCI)
				ehci_dump_all(0, NULL);
#endif /* defined(F12DUMPEHCI) */
			}
			break;
		}
		lk = *(u8 *)data;
	}
	return IOACT_CONT;
}
#endif

// checks if the current input on port 0x60 is from the keyboard or mouse,
// by polling the 5th bit on the first byte in port 0x64
bool is_input_from_keyboard(){
	u8 inputFrom0x64 = 0;
	u8 maskFor5bit = 1 << 5;

	do_io_default (IOTYPE_INB, 0x64, (void*)&inputFrom0x64);

	if((inputFrom0x64 & maskFor5bit) == 0) return true;
	else return false;
}

// parameter data: number of a key from the keyboard
// the function sets the special key flags 
void setSpecialKeysFlags(void *data){
	if(*(u8*)data == 29)
		{
			ctrl_down_flag = true;
		}
		if(*(u8*)data == 157)
		{
			ctrl_down_flag = false;
		}

		if(*(u8*)data == 42)
		{
			use_shift_flag = true;
		}
		if(*(u8*)data == 170)
		{
			use_shift_flag = false;
		}	
}

void setKeyCaptureMode(void *data){
	// this if happens only once, at the first time this function is called
	if(to_init_svga)
	{
		to_init_svga = false;
		Pdevice = malloc(sizeof(VmwareSvgaDevice));
		if(VmwareSvgaInit(Pdevice) == VMWARE_SVGA_DEVICE_NOT_FOUND){
			panic("@@@@@@@@@@@@ VmwareSvgaInit Failed : VMWARE_SVGA_DEVICE_NOT_FOUND @@@@@@@@@@@@");
		}
		// CURRENTLY LEAKING MEMORY HERE!!! (free Pdevice)
	}

	if(ctrl_down_flag && use_shift_flag && capture_keyboard_flag == false){
		capture_keyboard_flag = true;

		VmwareSvgaSwitchTo(Pdevice, false);
		if(VmwareSvgaPutString(Pdevice, "IN VGA", 10, 10) == VMWARE_SVGA_DEVICE_NOT_IN_VGA){
			panic("@@@@@@@@@@@@ VmwareSvgaPutString Failed : VMWARE_SVGA_DEVICE_NOT_IN_VGA @@@@@@@@@@@@");
		}
	}
	else if(ctrl_down_flag && use_shift_flag && capture_keyboard_flag == true){
		capture_keyboard_flag = false;

		VmwareSvgaSwitchTo(Pdevice, true);
	}
}

static enum ioact
my_kbd_handler (enum iotype type, u32 port, void *data)
{

	do_io_default (type, port, data);
	
	if(type == IOTYPE_INB){
		if(is_input_from_keyboard()){

			setSpecialKeysFlags(data);
			setKeyCaptureMode(data);
			
			if(capture_keyboard_flag && *(u8*)data < 128){
				if((scancode_to_asciichar[*(u8*)data] != -1) && !ctrl_down_flag){
					if(use_shift_flag){
						printf("%c\n", (char)scancode_to_asciichar_shift[*(u8*)data]);
					}
					else {
						printf("%c\n", (char)scancode_to_asciichar[*(u8*)data]);
					}
				}
			}
		}
	}

	return IOACT_CONT;
}

static void
setiohooks (void)
{	
	if (current->vcpu0 != current)
		return;

	set_iofunc (0x60, my_kbd_handler);

#ifdef DEBUG_IO_MONITOR
	set_iofunc (0x60, kbdio_monitor);
#endif
#ifdef DEBUG_IO0x20_MONITOR
	set_iofunc (0x20, io0x20_monitor);
#endif
#if defined (F11PANIC) || defined (F12MSG)
	if (config.vmm.f11panic || config.vmm.f12msg)
		set_iofunc (0x60, kbdio_dbg_monitor);
#endif
}

INITFUNC ("pass1", setiohooks);
