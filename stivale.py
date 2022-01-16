#!/usr/bin/env python3
import usb.core
import struct
import sys
import argparse
import time
import code

parser = argparse.ArgumentParser(description='A little stivale2 kernel/initrd uploader for pongoOS.')

parser.add_argument('-k', '--kernel', dest='kernel', help='path to kernel image')
parser.add_argument('-m', '--module', dest='module', help='path to a module')
parser.add_argument('-c', '--cmdline', dest='cmdline', help='set kernel command line')

args = parser.parse_args()

# if args.kernel is None:
#     print(f"error: No kernel specified! Run `{sys.argv[0]} --help` for usage.")
#     exit(1)

if args.cmdline is None:
    print(f"error: No cmdline specified! Run `{sys.argv[0]} --help` for usage.")
    exit(1)

if args.module is not None:
    print(f"todo: kernel modules!")
    exit(1)

dev = usb.core.find(idVendor=0x05ac, idProduct=0x4141)
if dev is None:
    print("Waiting for device...")

    while dev is None:
        dev = usb.core.find(idVendor=0x05ac, idProduct=0x4141)
        if dev is not None:
            dev.set_configuration()
            break
        time.sleep(2)
else:
    dev.set_configuration()

# kernel = open(args.kernel, "rb").read()
sabaton = open("/Users/pitust/code/fruity-sabaton/zig-out/bin/fruity_aarch64.bin", "rb").read()

if args.cmdline is not None:
    dev.ctrl_transfer(0x21, 4, 0, 0, 0)
    dev.ctrl_transfer(0x21, 3, 0, 0, f"linux_cmdline {args.cmdline}\n") # we aren't linux but who cares

if args.module is not None:
    print("Loading initial ramdisk...")
    module = open(args.module, "rb").read()
    module_size = len(module)
    dev.ctrl_transfer(0x21, 2, 0, 0, 0)
    dev.ctrl_transfer(0x21, 1, 0, 0, struct.pack('I', module_size))

    dev.write(2, module, 1000000)
    dev.ctrl_transfer(0x21, 4, 0, 0, 0)
    dev.ctrl_transfer(0x21, 3, 0, 0, "stivalemod\n")
    print("Module loaded successfully.")

print("Loading sabaton...")
sabaton_size = len(sabaton)
dev.ctrl_transfer(0x21, 2, 0, 0, 0)
dev.ctrl_transfer(0x21, 1, 0, 0, struct.pack('I', sabaton_size))

dev.write(2, sabaton, 1000000)
print("Sabaton loaded successfully.")

dev.ctrl_transfer(0x21, 4, 0, 0, 0)

print("Booting...")
dev.ctrl_transfer(0x21, 3, 0, 0, "go-stivale\n")

while True:
    print(dev.read(0x81, 1).tobytes())