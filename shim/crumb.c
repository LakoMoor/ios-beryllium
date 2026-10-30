/*
 * Debug log without a serial port: text in the ramoops console zone
 * (0xb0180000: dump zones 6 x 0x40000 first, then console; layout from the
 * live DT, record-size/console-size 0x40000, ecc 0). After a warm reset,
 * recovery's kernel exposes it as /sys/fs/pstore/console-ramoops-0.
 * persistent_ram_buffer: u32 sig 'DBGC', u32 start, u32 size, data[].
 */
#include <stdint.h>
#include "crumb.h"

#define CRUMB_BASE	0xb0180000UL
#define CRUMB_SIG	0x43474244U
#define CRUMB_DATA	(CRUMB_BASE + 12)
#define CRUMB_MAX	(0x40000 - 12)

static volatile uint32_t *hdr = (volatile uint32_t *)CRUMB_BASE;

/* byte stores only: this runs with the MMU off (Device memory) */
static void put(char c)
{
	uint32_t n = hdr[2];
	if (n >= CRUMB_MAX)
		return;
	*(volatile uint8_t *)(CRUMB_DATA + n) = (uint8_t)c;
	hdr[2] = n + 1;
	hdr[1] = n + 1;
}

void crumb_init(void)
{
	hdr[1] = 0;
	hdr[2] = 0;
	hdr[0] = CRUMB_SIG;
}

void crumb_str(const char *s)
{
	while (*s)
		put(*s++);
}

void crumb_hex(uint64_t v)
{
	static const char hex[] = "0123456789abcdef";
	for (int i = 60; i >= 0; i -= 4)
		put(hex[(v >> i) & 0xf]);
}

void crumb_line(const char *label, const char *s, uint64_t v)
{
	crumb_str(label);
	crumb_str(": ");
	crumb_str(s);
	crumb_hex(v);
	put('\n');
}

void crumb_start(uint64_t x0)
{
	extern char _head[];
	uint64_t el, mp;
	__asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
	__asm__ volatile("mrs %0, mpidr_el1" : "=r"(mp));
	crumb_line("start", "x0=", x0);
	crumb_line("start", "CurrentEL=", el);
	crumb_line("start", "mpidr=", mp);
	crumb_line("start", "loaded_at=", (uint64_t)_head);
}
