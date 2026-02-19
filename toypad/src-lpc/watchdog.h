/* vim: set tabstop=8 tw=76: */

// User manual 17.8.1 Watchdog mode register
#define WDMOD (*((volatile unsigned int *) 0x40004000))

// User manual 17.8.2 Watchdog Timer Constant register
#define WDTC (*((volatile unsigned int *) 0x40004004))

// User manual 17.8.3 Watchdog Feed register
#define WDFEED (*((volatile unsigned int *) 0x40004008))

// User manual 17.8.4 Watchdog Timer Value register
#define WDTV (*((volatile unsigned int *) 0x4000400C))

// User manual 17.8.5 Watchdog Clock Select register
#define WDCLKSEL (*((volatile unsigned int *) 0x40004010))

// User manual 17.8.6 Watchdog Timer Warning Interrupt register.
#define WDWARNINT (*((volatile unsigned int *) 0x40004014))

// User manual 17.8.7 Watchdog Timer Window register
#define WDWINDOW (*((volatile unsigned int *) 0x40004018))



// User manual 3.5.8 Watchdog oscillator control register
#define WDTOSCCTRL (*((volatile unsigned int *) 0x40008024))



// User manual 3.5.39 Wake-up configuration register
#define PDAWAKECFG (*((volatile unsigned int *) 0x40048234))

// User manual 3.5.40 Power configuration register
#define PDRUNCFG (*((volatile unsigned int *) 0x40048238))

// User manual Table 45. Wake-up configuration register (PDAWAKECFG, 
// address 0x4004 8234) bit description
// 
// User manual Table 46. Power configuration register (PDRUNCFG, address 
// 0x4004 8238) bit description
#define IRCOUT_PD (0)
#define IRC_PD (1)
#define FLASH_PD (2)
#define BOD_PD (3)
#define ADC_PD (4)
#define SYSOSC_PD (5)
#define WDTOSC_PD (6)
#define SYSPLL_PD (7)
#define USBPLL_PD (8)
#define USBPAD_PD (10)



// User manual Table 322. Watchdog mode register (MOD - 0x4000 4000) bit 
// description
#define WDEN (0)
#define WDRESET (1)
#define WDTOF (2)
#define WDINT (3)
#define WDPROTECT (4)
#define LOCK (5)



// User manual Table 327. Watchdog Clock Select register (CLKSEL - 
// 0x4000 4010) bit description
#define WDCLKSEL_CLKSEL (0)
#define WDCLKSEL_LOCK (31)
