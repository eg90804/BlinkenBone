/* main.c: RPC Server for BlinkenBus panel hardware interface.

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


   20-Feb-2016  JH	V 1.08 migration to VS2015 and repair
   08-Sep-2013	JH	V 1.07 Added check for duplicate panel names and duplicate control names
   01-Aug-2013	JH	V 1.06 Bugfix: board addr 0 was not allowed.
   22-Feb-2013	JH  V 1.05 On receive, control values are masked to valid bits.
   28-Jan-2013	JH  V 1.04 support for panel modes 1 and 2 for lamp and switch selftest
   30-Dec-2012  JH  V 1.03 Fixed back in blinkenbus.c, blinkenbus_read_panel_input_controls():
   				 range of used input registers was wrong calculated.
   07-Dec-2012	JH  V 1.02 Updated to handle the PDP-10 KI10 console:
   				- now 100 controls allowed
   				- Controls may have undefined lower bits
   				  (the KI10 PROGRAM COUNTER has only LEDs for Bits 35..18)
   08-Feb-2012  JH      created



   RPC Server for Blinkenlight panel hardware interface.

   To be run on a Beaglebone, must be compiled for ARM7 / Angstrom environment.

   Under WIN32, only operation is "test config file"

   Interfaces:
   1a) Interface to I/O boards
   - Wires of real blinkenlight panels are connected to
     I/O terminals on BLINKENBUS boards.
   - Voltages levels are set/read by accessing I/O registers on boards
     over BLINKENBUS
   - BLINKENBUS is a 8 bit address/ 8b it data bus, connected to Begalbeone GPIOs
   - kblinkenbus kernel module/driver maps IOBIS addrtess space to file interface,
   	 so sets of consecutive BLINKENBUS addresses can be easily accessed with
   	 file read()/write()/lseek()/ioctl() calls
   1b)
   	  Simulated panel: Interactive interface to user
   	  in mode "panelsim",  panel controls are not realized by REAL blinkenlight console
      but over user interface.
      In this mode, the application still runs as server, but
      running as daemon makes no sense.

   2) Panel definitions
     Application talk to the blinkenlight console in terms of
     "panels" and "controls".
     - Many blinkenlight panels can be run in parallel on different IO boards
     - a panel cosists of many "controls": highlevel user interface components
       Example: a otary switch with 4 positions, a row of 16 bit-switches for
       data word entry, a row of 22 LEDs for displaying a 22bit address, ....
     - A hierarchical configuration file ("blinkenlight panel config")
       defines panels, controls, and how controls are connected  to IOBIS I/O boards.
      - "one configuration file describes what is connected to one Beaglebone".
   3) Interface to applications over RPC
     - one server runs on one Beaglebone, but can serve multiple applications
        over a RPC (remote procedue call) API.
     - API function include
       "get control value", "set control value", "poll changed input controls"
       "get available panels" etc
     - a panel or control is identified by a "handle", which is defined in the
       configuration file.
 */


#define MAIN_C_


#define VERSION	"v1.07"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>

#ifdef WIN32
// getopt() not available under MSVC,
// but is included in ...\073oncrpc_win32\win32\rpcinfo\getopt.c
extern int	opterr;
extern int	optind;
extern int	optopt;
extern char	*optarg;
extern int getopt(argc, argv, opts) ;
#else
#include <unistd.h> // getopt()
#endif
#include "print.h"
#include "blinkenlight_panels.h"
#include "blinkenlight_api_server_procs.h"
#include "config.h"
#include "panelsim.h"

#include "main.h"

#ifdef WIN32
#else
///// for Blinkenlight API server
#include "blinkenbus.h"
#include "rpc_blinkenlight_api.h"
#include <rpc/pmap_clnt.h> // different name under "oncrpc for windows"?
#endif
//#include <memory.h>
//#include <sys/socket.h>
//#include <netinet/in.h>

#ifndef SIG_PF
#define SIG_PF void(*)(int)
#endif
///// end

//extern int stmode; // no server, just test config
static char program_info[1024];
static char program_name[1024]; // argv[0]
static char program_options[1024]; // argv[1.argc-1]


char configfilename[MAX_FILENAME_LEN];
static char logfilename[MAX_FILENAME_LEN];

// global flags
int mode_test; // no server, just test config
int mode_panelsim; // do not access BLINKENBUS, no daemon, user oeprates simualted panel

/*
 * print program info
 */
void info(void) {
	print(LOG_INFO, "\n");
#ifdef WIN32
	print(LOG_NOTICE,"blinkenlightd %s - crippled WIN32 version of server for Blinkenlight hardware interface \n", VERSION);
#else
	print(LOG_NOTICE,
			"*** blinkenlightd %s - server for BeagleBone blinkenlight panel interface ***\n", VERSION);
#endif
    print(LOG_NOTICE,"    Compiled " __DATE__ " " __TIME__ "\n");
    print(LOG_NOTICE,"    Copyright (C) 2012-2016 Joerg Hoppe.\n");
    print(LOG_NOTICE,"    Contact: j_hoppe@t-online.de\n");
    print(LOG_NOTICE,"    Web: www.retrocmp.com/projects/blinkenbone\n");
	print(LOG_NOTICE, "\n");
}

/*
 * print help
 */
static void help(void)
{
	fprintf(stderr, "\n");
#ifdef WIN32
	fprintf(stderr, "blinkenlightd %s - crippled WIN32 version of server for Blinkenlight hardware interface \n", VERSION);
	fprintf(stderr, "  (compiled " __DATE__ " " __TIME__ ")\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Call:\n");
	fprintf(stderr, "blinkenlightd -c <panel_config_file> -t\n");
#else
	fprintf(stderr, "blinkenlightd %s - RPC server for Blinkenlight hardware interface \n", VERSION);
	fprintf(stderr, "  (compiled " __DATE__ " " __TIME__ ")\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Call:\n");
	fprintf(stderr, "blinkenlightd -c <panel_config_file> [-l <log_file>] [-v] [-t]\n");
#endif
	fprintf(stderr, "\n");
	fprintf(stderr, "-c <config_file>: specify name of panel configuration file\n");
	fprintf(stderr,	"                  Defines, how panels are connected to BLINKENBUS interface boards\n");
#ifndef WIN32
//	fprintf(stderr, "- <port>:               TCP port for RCP access.\n") ;
	fprintf(stderr,
			"-b               : background operation: print to syslog (view with dmesg)\n");
	fprintf(stderr, "                  default output is stderr\n");
	fprintf(
			stderr,
			"-s               : do not operate on BLINKENBUS, show user GUI with simulated panel\n");
	fprintf(stderr, "                   (clears -b)\n");
	fprintf(stderr, "-v               : verbose: tell what I'm doing\n");
#endif
	fprintf(stderr,
			"-t               : go not into server mode, just test the config file\n");

	fprintf(stderr, "\n");
}

/*
 * read commandline paramaters into global vars
 * result: 1 = OK, 0 = error
 */
static int parse_commandline(int argc, char **argv)
{
	int i;
	int c;
	int opt_background = 0;

	strcpy(program_name, argv[0]);
	strcpy(program_options, "");
	for (i = 1; i < argc; i++)
	{
		if (i > 1)
			strcat(program_options, " ");
		strcat(program_options, argv[i]);
	}

	opterr = 0;

// clear vars
	strcpy(configfilename, "");
	mode_test = 0;
	mode_panelsim = 0;

#ifdef WIN32
#define OPTIONCODES "c:bvst"
#else
#define OPTIONCODES "c:vt"	// only -t
#endif
	while ((c = getopt(argc, argv, "c:bvst")) != -1)
		switch (c)
		{
		case 'c':
			strcpy(configfilename, optarg);
			break;
		case 'v':
			print_level = LOG_DEBUG;
			break;
		case 'b':
			opt_background = 1;
			break;
		case 's':
			mode_panelsim = 1;
			break;
		case 't':
			mode_test = 1;
			break;
		case '?': // getopt detected an error. "opterr=0", so own error message here
			if (optopt == 'c')
				fprintf(stderr, "Option -%c requires an argument.\n", optopt);
			else if (isprint(optopt))
				fprintf(stderr, "Unknown option `-%c'.\n", optopt);
			else
				fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
			return 0;
			break;
		default:
			abort(); // getopt() got crazy?
			break;
		}

	// all arguments set?
	if (!strlen(configfilename))
	{
		fprintf(stderr, "Missing mandatory arguments!\n");
		return 0;
	}
	if (mode_panelsim)
		opt_background = 0; // interactive simulation

	// start logging
	print_open(opt_background); // if background, then syslog

//     for (index = optind; index < argc; index++)
//       printf ("Non-option argument %s\n", argv[index]);
//     return 0;
	return 1;
}

#ifndef WIN32
/******************************************************
 * Server for Blinkenlight API
 * see code generated by rpcgen
 * DOES NEVER END!
 */
void blinkenlight_api_server(void)
{
	register SVCXPRT *transp;

// entry to server stub

	void blinkenlightd_1(struct svc_req *rqstp, register SVCXPRT *transp);

	pmap_unset(BLINKENLIGHTD, BLINKENLIGHTD_VERS);

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL)
	{
		print(LOG_ERR, "%s", "cannot create udp service.");
		exit(1);
	}
	if (!svc_register(transp, BLINKENLIGHTD, BLINKENLIGHTD_VERS, blinkenlightd_1, IPPROTO_UDP))
	{
		print(LOG_ERR, "%s", "unable to register (BLINKENLIGHTD, BLINKENLIGHTD_VERS, udp).");
		exit(1);
	}

	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL)
	{
		print(LOG_ERR, "%s", "cannot create tcp service.");
		exit(1);
	}
	if (!svc_register(transp, BLINKENLIGHTD, BLINKENLIGHTD_VERS, blinkenlightd_1, IPPROTO_TCP))
	{
		print(LOG_ERR, "%s", "unable to register (BLINKENLIGHTD, BLINKENLIGHTD_VERS, tcp).");
		exit(1);
	}

	// svc_run();
	// alternate implementation of svn_run() with periodically timeout and
	// 	calling of callback
	{
		fd_set readfds;
		struct timeval tv;
		int dtbsz = getdtablesize();
		for (;;)
		{
			readfds = svc_fdset;
			tv.tv_sec = 0;
			tv.tv_usec = 1000 * 2; // every 10 ms*time_slice_ms;
			switch (select(dtbsz, &readfds, NULL, NULL, &tv))
			{
			case -1:
				if (errno == EINTR)
					continue;
				perror("select");
				return;
			case 0: // timeout
					// provide the panel simulation with computing time
				if (mode_panelsim)
					panelsim_service();
				break;
			default:
				svc_getreqset(&readfds);
				break ;
			}
		}
	}

	print(LOG_ERR, "%s", "svc_run returned");
	exit(1);
	/* NOTREACHED */

}
#endif



// gets called when RPC client wants panel control values
static void on_blinkenlight_api_panel_get_controlvalues(blinkenlight_panel_t *p) {
/// read values from BLINKENBUS into input controls values
if (mode_panelsim)
	panelsim_read_panel_input_controls(p);
#ifndef WIN32
else
	blinkenbus_read_panel_input_controls(p);
#endif
}

// gets called when RPC client updated panel control values
static void on_blinkenlight_api_panel_set_controlvalues(blinkenlight_panel_t *p, int force_all) {
	/// write values of panel controls to BLINKENBUS. optimization: only changed
	if (mode_panelsim)
		panelsim_write_panel_output_controls(p, force_all);
#ifndef WIN32
	else
		blinkenbus_write_panel_output_controls(p, force_all);
#endif
}

static int on_blinkenlight_api_panel_get_state(blinkenlight_panel_t *p)
{
// set tristate of all BlinkenBoards of this panel.
// side effect to other panls on the same boards!
if (mode_panelsim)
	return panelsim_get_blinkenboards_state(p);
#ifndef WIN32
else
	return blinkenbus_get_blinkenboards_state(p);
#endif
}


static void on_blinkenlight_api_panel_set_state(blinkenlight_panel_t *p, int new_state)
{
// set tristate of all BlinkenBoards of this panel.
// side effect to other panels on the same boards!
if (mode_panelsim)
	panelsim_set_blinkenboards_state(p, new_state);
#ifndef WIN32
else
	blinkenbus_set_blinkenboards_state(p, new_state);
#endif
}


static char *on_blinkenlight_api_get_info() {
	static char buffer[1024] ;

sprintf(buffer, "Server info ...............: %s\n"
		"Server program name........: %s\n"
		"Server command line options: %s\n"
		"Server compile time .......: " __DATE__ " " __TIME__ "\n", //
		program_info,
		program_name,
		program_options);

	return buffer ;
}


/*
 *
 */
int main(int argc, char *argv[])
{

	print_level = LOG_NOTICE;
	// print_level = LOG_DEBUG;
	if (!parse_commandline(argc, argv))
	{
		help();
		return 1;
	}
	sprintf(program_info, "blinkenlightd - Blinkenlight API server daemon %s", VERSION) ;

	info() ;

	print(LOG_INFO, "Start\n");

	blinkenlight_panel_list = blinkenlight_panels_constructor();

	/*
	 * load panel configuration
	 */
	blinkenlight_panels_config_load(configfilename);

	blinkenlight_panels_config_fixup();

	// link set/get events
	blinkenlight_api_panel_get_controlvalues_evt = on_blinkenlight_api_panel_get_controlvalues ;
	blinkenlight_api_panel_set_controlvalues_evt = on_blinkenlight_api_panel_set_controlvalues ;
	blinkenlight_api_panel_get_state_evt = on_blinkenlight_api_panel_get_state ;
	blinkenlight_api_panel_set_state_evt = on_blinkenlight_api_panel_set_state ;
	blinkenlight_api_get_info_evt = on_blinkenlight_api_get_info ;

	if (mode_test)
	{
		blinkenlight_panels_diagprint(blinkenlight_panel_list, stderr);

		if (!blinkenlight_panels_config_check())
		{
			print(LOG_ERR, "Terminated with error.\n");
			exit(1);
		}
		print(LOG_INFO, "Terminated without error.\n");
	}
	else
	{
#ifdef WIN32
		print(LOG_ERR, "Under WIN32, only option \"-t: test config file\" is possible.\n");
#else
		if (mode_panelsim)
			panelsim_init(0); // at the moment, use first panel for simulation
		else
			blinkenbus_init(); // panel config must be known

		if (!blinkenlight_panels_config_check())
		{
			print(LOG_ERR, "Terminated with error.\n");
			exit(1);
		}
		/*
		 *  Enter API server mode
		 */
		print(LOG_NOTICE, "Starting Blinkenlight API server\n");
		blinkenlight_api_server();
		// does never end!
#endif
	}

	print_close(); // well ....

	blinkenlight_panels_destructor(blinkenlight_panel_list);

	return 0;
}