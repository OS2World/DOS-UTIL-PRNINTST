/*
**	TEST FOR PROPER INTERRUPT-DRIVEN OPERATION OF LPT1
**	
**	Written in MIcrosoft C Version 5.1 on Saturday October 14, 1989
*/

char copyr[] = "Copyright 1989 John Navas II, All Rights Reserved";

#include <bios.h>
#include <conio.h>
#include <dos.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>

#define TSTAMT	100						/* number of characters for test */

#define TIMEOUT	182						/* timer ticks for timeout */

#define STDERR	2						/* DOS standard error handle */
#define STDPRN	4						/* DOS printer handle */

#define PRNRDY	0x0F					/* printer ready interrupt */

#define INTA00	0x20					/* 8259 control port */
#define EOI		0x20					/* end of interrupt code */
#define INTA01	0x21					/* 8259 control port */
#define	PRN		0x80					/* printer interrupt bit */

#define PRNPORT	(*(unsigned far*)0x400008L)	/* LPT1 I/O port address */

#define PRNBUSY	0x80					/* printer NOT busy */
#define PRNSEL	0x10					/* printer selected */
#define OKSTAT	(PRNSEL | PRNBUSY)		/* printer ready */

#define LPT1	0x378					/* standard LPT1 address */

void (interrupt far* oldprn)() = 0L;	/* save old interrupt vector */

unsigned intcnt = 0;					/* count of interrupts */

/*
**	SERVICE PRINTER INTERRUPTS
*/
void interrupt far
service(void)
{
	++intcnt;							/* count interrupt */
	outp(INTA00, EOI);					/* end of interrupt to 8259 */
}

/*
**	CLEAN UP INTERRUPTS AT EXIT
*/
void
cleanup(void)
{
	unsigned char c;

	_disable();
	c = (char) inp(INTA01);
	c |= PRN;							/* disable printer ready interrupts */
	outp(INTA01, c);
	_enable();

	if (oldprn != 0L)					/* restore old interrupt vector */
		_dos_setvect(PRNRDY, oldprn);
}

/*
**	SETUP PRINTER INTERRUPT VECTOR AND ENABLE INTERRUPTS
*/
void
enable(void)
{
	unsigned char c;

	fprintf(stderr, "PRN base address is %3Xh\n", PRNPORT);
	if (!PRNPORT) {
		fprintf(stderr, "NO PRN PORT FOUND!\a\n");
		abort();
	}
	if (PRNPORT != LPT1)
		fprintf(stderr, "PRN address NON-STANDARD (should be %3Xh)!\a\n",
			LPT1);
	if (_bios_printer(_PRINTER_STATUS, 0, 0) != OKSTAT) {
		fprintf(stderr, "Printer not ready!\a\n");
		abort();
	}

	atexit(cleanup);
	oldprn = _dos_getvect(PRNRDY);		/* save old interrupt vector */
	_dos_setvect(PRNRDY, service);		/* install my interrupt vector */

	_disable();
	c = (char) inp(INTA01);
	c &= ~PRN;							/* enable printer ready interrupts */
	outp(INTA01, c);
	_enable();
}

/*
**	FORCE A BRIEF DELAY
*/
void
pause(void){}

/*
**	PRINT A CHARACTER ON LPT1 WITH INTERRUPTS ENABLED
*/
int										/* 0 : OK; non-zero : BUSY */
print(char c)
{
	outp(PRNPORT, c);					/* setup character */
	if (inp(PRNPORT+1) & PRNBUSY) {		/* printer not busy */
		outp(PRNPORT+2, 0x0D);			/* set printer strobe */
		pause();
		outp(PRNPORT+2, 0x1C);			/* reset strobe & enable interrupts */
		return 0;
	}
	else
		return -1;						/* not ready */
}

/*
**	RUN THE PRINTER TEST
*/
void
runtest(void)
{
	long t1, t2 = 0L;
	unsigned u, v;

	for (u = TSTAMT; u;) {				/* output character loop */
		_bios_timeofday(_TIME_GETCLOCK, &t1);
		if (t1 - t2 == 0)				/* wait at least one timer tick */
			continue;
		if (t1 - t2 >= TIMEOUT && t2 != 0) {	/* maximum time to wait */
timeout:
			fprintf(stderr, "\nPrinter time out!\a\n");
			abort();
		}
		if (print('\r'))				/* don't waste paper */
			continue;					/* not ready */
		write(STDERR, ".", 1);			/* display progress */
		t2 = t1;
		--u;							/* count output */
	}
	while (!(inp(PRNPORT+1) & PRNBUSY))	{	/* printer busy */
		_bios_timeofday(_TIME_GETCLOCK, &t1);
		if (t1 - t2 >= TIMEOUT)			/* maximum time to wait */
			goto timeout;
	}
}

main()
{
	fprintf(stderr,
		"Test for proper operation of LPT1 printer interrupts\n%s\n\n"
		"Boot system naked (i.e. with NO TSR's installed!)\n"
		"Ready printer on LPT1 and press any key to continue\n",
		copyr);
	getch();

	enable();
	
	runtest();

	fprintf(stderr, "\n\n%u interrupts detected for %u characters\n",
		intcnt, TSTAMT);
	if (TSTAMT != intcnt)
		fprintf(stderr, "%d INTERRUPTS LOST!\a\n", TSTAMT - intcnt);
	else if (PRNPORT == LPT1)
		fprintf(stderr, "Test completed successfully.\n");

	return 0;
}
