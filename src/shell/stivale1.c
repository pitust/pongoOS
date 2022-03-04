/*
 * stivale1 for pongoOS
 *
 * Copyright (C) 2022 pitust
 *
 * This file is part of stivale-pongoos.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#include <assert.h>
#include <pongo.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern uint32_t* ppage_list;
extern uint64_t ppages;
extern uint64_t pa_head;
extern void* fdt;
extern bool fdt_initialized;
void linux_dtree_init(void);

typedef struct {
    uint64_t addr;
    uint64_t len;
} mmap_t;

typedef struct {
    uint64_t cmdline;
    uint64_t mmap_len;
    mmap_t* mmap_base;
    uint64_t framebuffer_addr;
    uint64_t framebuffer_pitch;
    uint64_t framebuffer_width;
    uint64_t framebuffer_height;
    void* kernptr;
    uint64_t kernsz;
    void* dtb;
} fruitboot_t;

fruitboot_t fruitboot_info;

typedef void (*putchar_t)(char c);
typedef void (*fruitboot_blob_t)(putchar_t early_putc, fruitboot_t* info);

extern uint64_t ktrw_send_in_flight;

void usb_handler();
void usb_write_commit();
void do_usb_write(const char* buf) {
    // uint64_t towrite = strlen(buf);
    // uint64_t uncommited = 0;
    // wait until there is nothing in-flight...
    // while (ktrw_send_in_flight) usb_handler();
    // while (towrite) {
    //     uint64_t foo = usb_write(buf, towrite);
    //     towrite -= foo;
    //     uncommited += foo;
    //     if (!foo) {
    //         if (uncommited) usb_write_commit();
    //         uncommited = 0;
    //         while (ktrw_send_in_flight) usb_handler();
    //     }
    // }
    // while (ktrw_send_in_flight) usb_handler();
}
void plzz_putchar(char c) {
    char buf[2];
    buf[0] = c;
    buf[1] = 0;
    fprintf(stderr, "%c", c);
    do_usb_write(buf);
}
volatile void jump_to_image_extended(uint64_t image, uint64_t args, uint64_t original_image);
void go_stivale() {
    iprintf("stivale1: it's go time!\n");
    // linux_dtree_init();
    iprintf("got a dtb...\n");
    fruitboot_info.dtb = fdt;
    // loader_xfer_recv_data

    mmap_t* entries = malloc(sizeof(mmap_t) * 256);
    fruitboot_info.mmap_base = entries;
    uint64_t entcount = 256;
    uint64_t cur_ent = 0;
    mmap_t* current = NULL;
    for (uint64_t i = 0; i < ppages; i++) {
        uint64_t addr = (i << 14ULL) + gBootArgs->physBase;
        if (addr >= 0x810000000 && addr <= 0x810000000 + loader_xfer_recv_count) {
            if (ppage_list[i] != 0) {
                do_usb_write("ERROR: sabaton load failed: the range is not free!\n");
                iprintf("ERROR: sabaton load failed: the range is not free!\n");
                return;
            }
            continue;
        }
        if (ppage_list[i] == 0) {
            if (!current) {
                current = &entries[cur_ent++];
                if (cur_ent > entcount) {
                    task_crash_asserted("ERROR: too much mmap entries!");
                }
                current->addr = addr;
                current->len = 0;
            }
            current->len += 1 << 14;
        } else {
            current = NULL;
        }
    }
    fruitboot_info.mmap_len = cur_ent;

    for (int i = 0; i < cur_ent; i++) {
        iprintf(" %3d memory: %llx -> %llx\n", i, entries[i].addr, entries[i].addr + entries[i].len);
    }

    iprintf("fb @ %p %lux%lu stride=%lu\n",
            (void*)gBootArgs->Video.v_baseAddr,
            gBootArgs->Video.v_width,
            gBootArgs->Video.v_height,
            gBootArgs->Video.v_rowBytes);
    if (!is_16k()) {
        task_crash_asserted("ERROR: device uses 4k paging!");
    }
    for (uint64_t i = 0; i < ppages; i++) {
        uint64_t addr = (i << 14ULL) + gBootArgs->physBase;
        if (addr >= 0x810000000 && addr <= 0x810000000 + loader_xfer_recv_count) {
            if (ppage_list[i] != 0) {
                do_usb_write("ERROR: sabaton load failed: the range is not free!\n");
                iprintf("ERROR: sabaton load failed: the range is not free!\n");
                return;
            }
            continue;
        }
        ppage_list[i] = 1;
    }
    pa_head = 0;
    iprintf("froze the PMM!\n");
    memcpy((void*)0x810000000, loader_xfer_recv_data, loader_xfer_recv_count);
    // 64 is supposedly the cache line size on arm
    uint64_t sabaton_end = (0x810000000 + loader_xfer_recv_count + 63) & ~63;
    iprintf("copied sabaton!\n");
    for (uint32_t i = 0; i < 8; i++) {
        iprintf("hax: 0x%2x: %8llx!\n", i, ((uint64_t*)(0x810000000))[i]);
    }

    disable_interrupts();

    ktrw_send_in_flight = 0;
    uint64_t towrite = 3;
    while (towrite) {
        fprintf(stderr, "usb_write loop go...\n");
        uint64_t foo = usb_write("foo", towrite);
        towrite -= foo;
        fprintf(stderr, "usb_write loop go...\n");
        if (!foo) {
            usb_write_commit();
            fprintf(stderr, "did not write, polling USB...\n");
            while (ktrw_send_in_flight) usb_handler();
            fprintf(stderr, "USB poll complete...\n");
        }
    }
    fprintf(stderr, "final usb poll...\n");
    while (ktrw_send_in_flight) usb_handler();
    fprintf(stderr, "sent data!\n");


    sep_teardown();
    __asm__ volatile("dsb sy");
    fprintf(stderr, "interrupts cleared!\n");
    // here we do final teardown, and jump to sabaton!
    // gFramebuffer = (uint32_t*)gBootArgs->Video.v_baseAddr;

    plzz_putchar('?');
    fruitboot_blob_t blob = (fruitboot_blob_t)(0x810000000ULL);
    for (uint64_t i = 0x810000000; i < sabaton_end; i += 64) {
        asm volatile(
            "DC CVAU, %[flushptr]\n"
            "DSB NSHST\n"
            "IC IVAU, %[flushptr]\n"
            "DSB NSH\n"
            "ISB\n" ::[flushptr] "r"(i)
            : "memory");
    }

    asm volatile(
        "isb\n"
        "dsb sy\n"
        "ic iallu\n"
        "dsb sy\n"
        "isb\n" ::
            : "memory");
    fprintf(stderr, "please do work...\n");
    blob(plzz_putchar, &fruitboot_info);
    fprintf(stderr, "it's over???\n");
    while (1) {
    }
}

void stivale2_commands_register() {
    command_register("go-stivale", "does stivale shit", go_stivale);
}
