/*
 * (C) Copyright 2008 Texas Insturments
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002
 * Gary Jennejohn, DENX Software Engineering, <garyj@denx.de>
 *
 * Copyright (C) 2010-2012 Freescale Semiconductor, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * CPU specific code
 */

#include <common.h>
#include <command.h>
#if defined(CONFIG_VIDEO_MX5) && (defined(CONFIG_MX53) || \
				  defined(CONFIG_MX51_BBG))
#include <ipu.h>
#endif
#include <asm/system.h>
#include <asm/cache.h>
#include <asm/cache-cp15.h>
#include <asm/pl310.h>
#include <asm/io.h>
#include <asm/mmu.h>

#ifndef CONFIG_L2_OFF
#ifndef CONFIG_MXC
#include <asm/arch/sys_proto.h>
#endif
#endif

#define cache_flush(void)	\
{	\
	asm volatile (	\
		"stmfd sp!, {r0-r5, r7, r9-r11};"	\
		"mrc        p15, 1, r0, c0, c0, 1;" /*@ read clidr*/	\
		/* @ extract loc from clidr */       \
		"ands       r3, r0, #0x7000000;"	\
		/* @ left align loc bit field*/      \
		"mov        r3, r3, lsr #23;"	\
		/* @ if loc is 0, then no need to clean*/    \
		"beq        555f;" /* finished;" */	\
		/* @ start clean at cache level 0*/  \
		"mov        r10, #0;"	\
		"111:" /*"loop1: */	\
		/* @ work out 3x current cache level */	\
		"add        r2, r10, r10, lsr #1;"	\
		/* @ extract cache type bits from clidr */    \
		"mov        r1, r0, lsr r2;"	\
		/* @ mask of the bits for current cache only */	\
		"and        r1, r1, #7;"	\
		/* @ see what cache we have at this level*/  \
		"cmp        r1, #2;"	\
		/* @ skip if no cache, or just i-cache*/	\
		"blt        444f;" /* skip;" */	\
		/* @ select current cache level in cssr*/   \
		"mcr        p15, 2, r10, c0, c0, 0;"	\
		/* @ isb to sych the new cssr&csidr */	\
		"mcr        p15, 0, r10, c7, c5, 4;"	\
		/* @ read the new csidr */    \
		"mrc        p15, 1, r1, c0, c0, 0;"	\
		/* @ extract the length of the cache lines */ \
		"and        r2, r1, #7;"	\
		/* @ add 4 (line length offset) */   \
		"add        r2, r2, #4;"	\
		"ldr        r4, =0x3ff;"	\
		/* @ find maximum number on the way size*/   \
		"ands       r4, r4, r1, lsr #3;"	\
		/*"clz  r5, r4;" @ find bit position of way size increment*/ \
		".word 0xE16F5F14;"	\
		"ldr        r7, =0x7fff;"	\
		/* @ extract max number of the index size*/  \
		"ands       r7, r7, r1, lsr #13;"	\
		"222:" /* loop2:"  */	\
		/* @ create working copy of max way size*/   \
		"mov        r9, r4;"	\
		"333:" /* loop3:"  */	\
		/* @ factor way and cache number into r11*/  \
		"orr        r11, r10, r9, lsl r5;"	\
		/* @ factor index number into r11*/  \
		"orr        r11, r11, r7, lsl r2;"	\
		/* @ clean & invalidate by set/way */	\
		"mcr        p15, 0, r11, c7, c14, 2;"	\
		/* @ decrement the way */	\
		"subs       r9, r9, #1;"	\
		"bge        333b;" /* loop3;" */	\
		/* @ decrement the index */	\
		"subs       r7, r7, #1;"	\
		"bge        222b;" /* loop2;" */	\
		"444:" /* skip: */	\
		/*@ increment cache number */	\
		"add        r10, r10, #2;"	\
		"cmp        r3, r10;" 	\
		"bgt        111b;" /* loop1; */	\
		"555:" /* "finished:" */	\
		/* @ swith back to cache level 0 */	\
		"mov        r10, #0;"	\
		/* @ select current cache level in cssr */	\
		"mcr        p15, 2, r10, c0, c0, 0;"	\
		/* @ isb to sych the new cssr&csidr */	\
		"mcr        p15, 0, r10, c7, c5, 4;" 	\
		"ldmfd 	    sp!, {r0-r5, r7, r9-r11};"	\
		"666:" /* iflush:" */	\
		"mov        r0, #0x0;"	\
		/* @ invalidate I+BTB */	\
		"mcr        p15, 0, r0, c7, c5, 0;" 	\
		/* @ drain WB */	\
		"mcr        p15, 0, r0, c7, c10, 4;"	\
		:	\
		:	\
		: "r0" /* Clobber list */	\
	);	\
}

int cleanup_before_linux(void)
{
	unsigned int i;

#ifdef CONFIG_CMD_IMX_DOWNLOAD_MODE
	extern void clear_mfgmode_mem(void);

	clear_mfgmode_mem();
#endif

#if defined(CONFIG_VIDEO_MX5) && (defined(CONFIG_MX53) || \
				  defined(CONFIG_MX51_BBG))
	ipu_disable_channel(MEM_BG_SYNC);
	ipu_uninit_channel(MEM_BG_SYNC);
#endif

	/*
	 * this function is called just before we call linux
	 * it prepares the processor for linux
	 *
	 * we turn off caches etc ...
	 */
	disable_interrupts();

	/* flush cache */
	cache_flush();

	/* turn off I/D-cache */
	icache_disable();
	/* invalidate D-cache */
	dcache_disable();

#ifndef CONFIG_L2_OFF
	/* turn off L2 cache */
	l2_cache_disable();
	/* invalidate L2 cache also */
	v7_flush_dcache_all(get_device_type());
#endif
	i = 0;
	/* mem barrier to sync up things */
	asm("mcr p15, 0, %0, c7, c10, 4": :"r"(i));

	/* turn off MMU */
	MMU_OFF();

#ifndef CONFIG_L2_OFF
	l2_cache_enable();
#endif

	return 0;
}

struct pl310_regs *const pl310 = (struct pl310_regs *)CONFIG_SYS_PL310_BASE;

static void pl310_cache_sync(void)
{
        writel(0, &pl310->pl310_cache_sync);
}



/* invalidate memory from start to stop-1 */
void v7_outer_cache_inval_range(u32 start, u32 stop)
{
        /* PL310 currently supports only 32 bytes cache line */
        u32 pa, line_size = 32;

        /*
         * If start address is not aligned to cache-line do not
         * invalidate the first cache-line
         */
        if (start & (line_size - 1)) {
                printf("ERROR: %s - start address is not aligned - 0x%08x\n",
                        __func__, start);
                /* move to next cache line */
                start = (start + line_size - 1) & ~(line_size - 1);
        }

        /*
        * If stop address is not aligned to cache-line do not
         * invalidate the last cache-line
         */
        if (stop & (line_size - 1)) {
               printf("ERROR: %s - stop address is not aligned - 0x%08x\n",
                        __func__, stop);
                /* align to the beginning of this cache line */
                stop &= ~(line_size - 1);
        }

        for (pa = start; pa < stop; pa = pa + line_size)
                writel(pa, &pl310->pl310_inv_line_pa);

        pl310_cache_sync();
}


/* Flush(clean invalidate) memory from start to stop-1 */
void v7_outer_cache_flush_range(u32 start, u32 stop)
{
        /* PL310 currently supports only 32 bytes cache line */
        u32 pa, line_size = 32;

        /*
         * If start address is not aligned to cache-line do not
         * invalidate the first cache-line
         */
        if (start & (line_size - 1)) {
                printf("ERROR: %s - start address is not aligned - 0x%08x\n",
                        __func__, start);
                /* move to next cache line */
                start = (start + line_size - 1) & ~(line_size - 1);
        }

        /*
         * If stop address is not aligned to cache-line do not
         * invalidate the last cache-line
         */
        if (stop & (line_size - 1)) {
                printf("ERROR: %s - stop address is not aligned - 0x%08x\n",
                        __func__, stop);
                /* align to the beginning of this cache line */
                stop &= ~(line_size - 1);
        }

                /*
         * Align to the beginning of cache-line - this ensures that
         * the first 5 bits are 0 as required by PL310 TRM
        */
        start &= ~(line_size - 1);

        for (pa = start; pa < stop; pa = pa + line_size)
                writel(pa, &pl310->pl310_clean_inv_line_pa);

        pl310_cache_sync();
}


static u32 get_ccsidr(void)
{
        u32 ccsidr;

        /* Read current CP15 Cache Size ID Register */
        asm volatile ("mrc p15, 1, %0, c0, c0, 0" : "=r" (ccsidr));
        return ccsidr;
}


static void v7_dcache_clean_inval_range(u32 start, u32 stop, u32 line_len)
{
        u32 mva;

        /* Align start to cache line boundary */
        start &= ~(line_len - 1);
        for (mva = start; mva < stop; mva = mva + line_len) {
                /* DCCIMVAC - Clean & Invalidate data cache by MVA to PoC */
                asm volatile ("mcr p15, 0, %0, c7, c14, 1" : : "r" (mva));
        }
}

static void v7_dcache_inval_range(u32 start, u32 stop, u32 line_len)
{
       u32 mva;

        /*
         * If start address is not aligned to cache-line do not
         * invalidate the first cache-line
         */
        if (start & (line_len - 1)) {
                printf("ERROR: %s - start address is not aligned - 0x%08x\n",
                        __func__, start);
                /* move to next cache line */
                start = (start + line_len - 1) & ~(line_len - 1);
        }

        /*
         * If stop address is not aligned to cache-line do not
         * invalidate the last cache-line
         */
        if (stop & (line_len - 1)) {
                printf("ERROR: %s - stop address is not aligned - 0x%08x\n",
                        __func__, stop);
                /* align to the beginning of this cache line */
                stop &= ~(line_len - 1);
        }

        for (mva = start; mva < stop; mva = mva + line_len) {
                /* DCIMVAC - Invalidate data cache by MVA to PoC */
                asm volatile ("mcr p15, 0, %0, c7, c6, 1" : : "r" (mva));
        }
}



static void v7_dcache_maint_range(u32 start, u32 stop, u32 range_op)
{
        u32 line_len, ccsidr;

        ccsidr = get_ccsidr();
        line_len = ((ccsidr & CCSIDR_LINE_SIZE_MASK) >>
                        CCSIDR_LINE_SIZE_OFFSET) + 2;
        /* Converting from words to bytes */
        line_len += 2;
        /* converting from log2(linelen) to linelen */
        line_len = 1 << line_len;

        switch (range_op) {
        case ARMV7_DCACHE_CLEAN_INVAL_RANGE:
                v7_dcache_clean_inval_range(start, stop, line_len);
                break;
        case ARMV7_DCACHE_INVAL_RANGE:
                v7_dcache_inval_range(start, stop, line_len);
                break;
        }

        /* DSB to make sure the operation is complete */
        CP15DSB;
}


/*
 * Flush range(clean & invalidate) from all levels of D-cache/unified
 * cache used:
 * Affects the range [start, stop - 1]
 */
void flush_dcache_range(unsigned long start, unsigned long stop)
{
        v7_dcache_maint_range(start, stop, ARMV7_DCACHE_CLEAN_INVAL_RANGE);

        v7_outer_cache_flush_range(start, stop);
}

/*
 * Invalidates range in all levels of D-cache/unified cache used:
 * Affects the range [start, stop - 1]
 */
void invalidate_dcache_range(unsigned long start, unsigned long stop)
{
        v7_dcache_maint_range(start, stop, ARMV7_DCACHE_INVAL_RANGE);

        v7_outer_cache_inval_range(start, stop);
}

