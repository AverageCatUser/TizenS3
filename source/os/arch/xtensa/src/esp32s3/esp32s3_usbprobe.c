/* Fully RAM-resident boot probe. Code is in IRAM, all strings are built on
 * the stack (no .rodata in flash), and it is the FIRST thing __start calls,
 * before up_irq_disable() or anything else that lives in flash. It prints
 * the PROCPU reset cause, then a heartbeat, over both USB-Serial-JTAG and
 * UART0, feeding every watchdog, forever, with no flash access at all.
 *
 * If this still resets the board, the fault is in the bootloader->app
 * hand-off or the flash image itself, not in any C code I wrote.
 */
#include <stdint.h>
#include "xtensa_attr.h"
#include "chip/esp32s3_soc.h"

#define DRAM_STR __attribute__((section(".dram1")))
static const char g_msg[] DRAM_STR __attribute__((unused)) = "\r\n=== TizenS3 USB PROBE ok, boot reached __start ===\r\n";

#define RTC_RESET_STATE_REG  (DR_REG_RTCCNTL_BASE + 0x38)
#define RTC_CNTL_RESET_CAUSE_PROCPU_V  0x3f

#define USJ_EP1   (*(volatile uint32_t *)USB_SERIAL_JTAG_EP1_REG)
#define USJ_CONF  (*(volatile uint32_t *)USB_SERIAL_JTAG_EP1_CONF_REG)
#define U0_FIFO   (*(volatile uint32_t *)UART_FIFO_REG(0))
#define U0_STAT   (*(volatile uint32_t *)UART_STATUS_REG(0))

static void IRAM_ATTR p_putc(char c)
{
    int t;
    /* USB-Serial-JTAG */
    for (t = 0; t < 80000; t++) {
        if (USJ_CONF & USB_SERIAL_JTAG_IN_EP_DATA_FREE) { USJ_EP1 = (uint8_t)c; break; }
        if (t == 2000) USJ_CONF = USB_SERIAL_JTAG_WR_DONE;
    }
    /* UART0 */
    for (t = 0; t < 80000; t++) {
        if (UART_TXFIFO_CNT(U0_STAT) < UART_TXFIFO_SIZE - 1) { U0_FIFO = (uint8_t)c; break; }
    }
}

static void IRAM_ATTR p_flush(void) { USJ_CONF = USB_SERIAL_JTAG_WR_DONE; }

static void IRAM_ATTR p_hex(uint32_t v)
{
    for (int i = 7; i >= 0; i--) {
        uint32_t nib = (v >> (i * 4)) & 0xf;
        p_putc(nib < 10 ? '0' + nib : 'a' + nib - 10);
    }
}

static void IRAM_ATTR feed_wdts(void)
{
    volatile uint32_t *r;
    r = (volatile uint32_t *)RTC_CNTL_WDTWPROTECT_REG; *r = RTC_CNTL_WDT_WKEY_VALUE;
    *(volatile uint32_t *)RTC_CNTL_WDTCONFIG0_REG = 0;
    *r = 0;
    for (int i = 0; i < 2; i++) {
        *(volatile uint32_t *)TIMG_WDTWPROTECT_REG(i) = TIMG_WDT_WKEY_VALUE;
        *(volatile uint32_t *)TIMG_WDTCONFIG0_REG(i) = 0;
        *(volatile uint32_t *)TIMG_WDTWPROTECT_REG(i) = 0;
    }
    r = (volatile uint32_t *)RTC_CNTL_SWD_WPROTECT_REG; *r = RTC_CNTL_SWD_WKEY_VALUE;
    *(volatile uint32_t *)RTC_CNTL_SWD_CONF_REG |= RTC_CNTL_SWD_AUTO_FEED_EN;
    *r = 0;
}

/* build a short string on the stack, no flash .rodata */
static void IRAM_ATTR p_tag(void)
{
    char s[6];
    s[0]='\r'; s[1]='\n'; s[2]='T'; s[3]='S'; s[4]='3'; s[5]=' ';
    for (int i = 0; i < 6; i++) p_putc(s[i]);
}

void IRAM_ATTR esp32s3_usbprobe(void)
{
    uint32_t cause = (*(volatile uint32_t *)RTC_RESET_STATE_REG) & RTC_CNTL_RESET_CAUSE_PROCPU_V;
    uint32_t n = 0;

    for (;;) {
        feed_wdts();
        p_tag();
        /* "rst=XX " then "n=XXXXXXXX" */
        p_putc('r'); p_putc('s'); p_putc('t'); p_putc('=');
        p_hex(cause);
        p_putc(' '); p_putc('n'); p_putc('=');
        p_hex(n++);
        p_flush();
        for (volatile int d = 0; d < 3000000; d++) { }
    }
}


/* ---- RTC-memory boot record (survives every reset except power-on) ---- */
#define RTCREC_BASE   0x50001f00u          /* top of 8 KB RTC slow memory */
#define RTCREC_MAGIC  0x54533342u          /* "TS3B" */
#define RTCREC(i)     (*(volatile uint32_t *)(RTCREC_BASE + 4u * (i)))
/* [0]=magic [1]=last cp [2]=crash code [3]=crash pc [4]=crash va [5]=boot count */

static void IRAM_ATTR p_str4(char a, char b, char c, char d) { p_putc(a); p_putc(b); p_putc(c); p_putc(d); }

/* Called at checkpoint A: report the previous boot, wait so a host can attach */
static void IRAM_ATTR report_previous_boot(uint32_t cause)
{
    uint32_t valid = (RTCREC(0) == RTCREC_MAGIC);
    uint32_t prev_cp = valid ? RTCREC(1) : 0;
    uint32_t c = valid ? RTCREC(2) : 0, pc = valid ? RTCREC(3) : 0, va = valid ? RTCREC(4) : 0;
    uint32_t boots = valid ? RTCREC(5) + 1 : 1;

    RTCREC(0) = RTCREC_MAGIC; RTCREC(1) = 'A'; RTCREC(2) = 0; RTCREC(3) = 0; RTCREC(4) = 0; RTCREC(5) = boots;

    for (int k = 0; k < 12; k++) {               /* ~6 s total */
        feed_wdts();
        p_tag();
        p_str4('b','o','o','t'); p_putc('='); p_hex(boots);
        p_putc(' '); p_str4('r','s','t','='); p_hex(cause);
        p_putc(' '); p_str4('p','r','e','v'); p_putc('='); p_putc(prev_cp ? (char)prev_cp : '-');
        if (c) {
            p_putc(' '); p_putc('c'); p_putc('='); p_hex(c);
            p_putc(' '); p_putc('p'); p_putc('c'); p_putc('='); p_hex(pc);
            p_putc(' '); p_putc('v'); p_putc('a'); p_putc('='); p_hex(va);
        }
        p_flush();
        for (volatile int d = 0; d < 2500000; d++) { }
    }
}

/* Boot trace: print "TS3 cp=X rst=XXXXXXXX" over raw USB-Serial-JTAG and
 * return (does not halt). Safe before the OS/console exists.
 */
void IRAM_ATTR esp32s3_trace(char tag)
{
    uint32_t cause = (*(volatile uint32_t *)RTC_RESET_STATE_REG) & RTC_CNTL_RESET_CAUSE_PROCPU_V;
    if (tag == 'A') {
        report_previous_boot(cause);
    }
    RTCREC(1) = (uint32_t)tag;
    p_tag();
    p_putc('c'); p_putc('p'); p_putc('='); p_putc(tag);
    p_putc(' '); p_putc('r'); p_putc('s'); p_putc('t'); p_putc('=');
    p_hex(cause);
    p_flush();
    for (volatile int d = 0; d < 300000; d++) { }   /* let USB drain */
}

/* Crash report over raw USB: "TS3 CRASH c=<code> pc=<epc> va=<excvaddr>" */
void IRAM_ATTR esp32s3_trace_crash(uint32_t code, uint32_t pc, uint32_t va)
{
    RTCREC(0) = RTCREC_MAGIC; RTCREC(2) = code ? code : 0xffu; RTCREC(3) = pc; RTCREC(4) = va;
    for (int k = 0; k < 3; k++) {          /* repeat so a late-opened terminal still sees it */
        p_tag();
        p_putc('C'); p_putc('R'); p_putc('A'); p_putc('S'); p_putc('H');
        p_putc(' '); p_putc('c'); p_putc('='); p_hex(code);
        p_putc(' '); p_putc('p'); p_putc('c'); p_putc('='); p_hex(pc);
        p_putc(' '); p_putc('v'); p_putc('a'); p_putc('='); p_hex(va);
        p_flush();
        for (volatile int d = 0; d < 3000000; d++) { }
    }
}
