/* blinkenbus.h: Access to BlinkenBus registers

   Copyright (c) 2012-2016, Joerg Hoppe
   j_hoppe@t-online.de, www.retrocmp.com

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   JOERG HOPPE BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


   17-Feb-2012  JH      created
*/


#ifndef BLINKENBUS_H_
#define BLINKENBUS_H_

#include <stdio.h>
#include "rpc_blinkenlight_api.h" // param value const
#include "blinkenlight_panels.h"

/**** these are duplicates from the kblinkenbus driver sources ***/
#define DEVICE_FILE_NAME "blinkenbus" // should be used in /dev/
#define BLINKENBUS_MAX_BOARD_ADDR	0x1f	// valid boards adresses are 0 to 31
#define BLINKENBUS_MAX_REGISTER_ADDR	0x1ff	// 9 bit BLINKENBUS addresses

#define BLINKENBUS_ADDRESS_IO(board,reg) (((board) << 4) | ((reg) & 0xf))	// get adr of io registers
#define BLINKENBUS_ADDRESS_CONTROL(board)	BLINKENBUS_ADDRESS_IO(board,0xf)	// reg 15 is control reg

#define BLINKENBUS_INPUT_LOWPASS_F_CUTOFF 20000 // input filtershave low pass for 20kHz


#ifndef BLINKENBUS_C_
extern int blinkenbus_fd; // file descriptor for interface to driver
#endif


void blinkenbus_init(void);
void blinkenbus_read_panel_input_controls(blinkenlight_panel_t *p);
void blinkenbus_write_panel_output_controls(blinkenlight_panel_t *p, int force_all);

void blinkenbus_set_blinkenboards_state(blinkenlight_panel_t *p, int new_state) ;
int blinkenbus_get_blinkenboards_state(blinkenlight_panel_t *p) ;

#endif /* BLINKENBUS_H_ */