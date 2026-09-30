#!/usr/bin/env python3
"""Compile the real ChibiOS receive functions against a deterministic queue.

Run with a host C compiler. --source-ref can demonstrate the regression in an
older commit without checking out or modifying its files.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    import re
    match = re.search(r"^(?:bool|size_t|void) " + name + r"\([^\n]*\) \{", source, re.M)
    if match is None:
        return ""
    start = match.start()
    depth = 0
    for end in range(source.index("{", start), len(source)):
        depth += (source[end] == "{") - (source[end] == "}")
        if depth == 0:
            return source[start:end + 1]
    raise ValueError(f"Unclosed function: {name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-ref")
    args = parser.parse_args()

    def read(path):
        if args.source_ref:
            return subprocess.check_output(["git", "show", f"{args.source_ref}:{path}"], cwd=ROOT, text=True)
        return (ROOT / path).read_text(encoding="utf-8")

    driver = read("tmk_core/protocol/chibios/usb_driver.c")
    main_source = read("tmk_core/protocol/chibios/usb_main.c")
    prefix = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CDC_EPSIZE 64
#define USB_ACTIVE 1
#define TIME_IMMEDIATE 0
#define TIME_INFINITE -1
#define USB_ENDPOINT_OUT_CDC_DATA 0
#define USB_ENDPOINT_IN_CDC_DATA 0
#define osalDbgCheck(x) assert(x)
#define osalSysLock() ((void)0)
#define osalSysUnlock() ((void)0)
typedef int sysinterval_t;
typedef struct { uint8_t data[1024]; size_t length, position; } fake_queue_t;
typedef struct { struct { void *usbp; } config; bool timed_out; fake_queue_t ibqueue; } usb_endpoint_out_t;
static usb_endpoint_out_t usb_endpoints_out[1];
static int active = USB_ACTIVE;
static uint8_t output[1024];
static size_t output_length;
static int usbGetDriverStateI(void *driver) { (void)driver; return active; }
static void flush_report_buffered(int endpoint, bool force) { (void)endpoint; (void)force; }
static size_t ibqReadTimeout(fake_queue_t *queue, uint8_t *data, size_t size, sysinterval_t timeout) {
    (void)timeout;
    size_t available = queue->length - queue->position;
    size_t received = available < size ? available : size;
    memcpy(data, queue->data + queue->position, received);
    queue->position += received;
    return received;
}
static void virtser_recv(uint8_t c) {
    assert(output_length < sizeof(output));
    output[output_length++] = c;
}
'''
    compatibility = r'''
static bool __attribute__((unused)) receive_report(int index, void *buffer, size_t size) {
    return usb_endpoint_out_receive(&usb_endpoints_out[index], buffer, size, TIME_IMMEDIATE);
}
'''
    checks = r'''
static void load_data(size_t length) {
    fake_queue_t *queue = &usb_endpoints_out[0].ibqueue;
    queue->length = length;
    queue->position = 0;
    for (size_t i = 0; i < length; ++i) queue->data[i] = (uint8_t)(i * 17 + 3);
    output_length = 0;
}
int main(void) {
    const size_t sizes[] = {0, 1, 3, 6, 31, 63, 64, 65, 127, 128, 129, 511};
    for (size_t test = 0; test < sizeof(sizes) / sizeof(sizes[0]); ++test) {
        load_data(sizes[test]);
        virtser_task();
        if (output_length != sizes[test]) {
            fprintf(stderr, "Lost serial data: sent=%zu received=%zu\n", sizes[test], output_length);
            return 1;
        }
        assert(memcmp(output, usb_endpoints_out[0].ibqueue.data, sizes[test]) == 0);
        virtser_task();
        assert(output_length == sizes[test]); /* no replay after queue drains */
    }
    /* A timeout must not discard the next short burst. */
    load_data(0); virtser_task();
    load_data(5); virtser_task(); assert(output_length == 5);
    /* Inactive USB consumes no queued input. */
    load_data(7); active = 0; virtser_task();
    assert(output_length == 0 && usb_endpoints_out[0].ibqueue.position == 0);
    active = USB_ACTIVE; virtser_task(); assert(output_length == 7);
    /* VIA/MIDI callers retain the original full-report boolean contract. */
    uint8_t buffer[64];
    load_data(3); assert(!usb_endpoint_out_receive(&usb_endpoints_out[0], buffer, sizeof(buffer), TIME_IMMEDIATE));
    load_data(64); assert(usb_endpoint_out_receive(&usb_endpoints_out[0], buffer, sizeof(buffer), TIME_IMMEDIATE));
    puts("PASS: 12 packet sizes, no replay, timeout recovery, USB inactivity, fixed-report compatibility");
    return 0;
}
'''
    source = prefix + function(driver, "usb_endpoint_out_receive_bytes") + "\n" + function(driver, "usb_endpoint_out_receive") + compatibility + function(main_source, "virtser_task") + checks
    with tempfile.TemporaryDirectory(prefix="qmk-virtser-") as temp:
        c_file = Path(temp) / "receive.c"
        executable = Path(temp) / ("receive.exe" if os.name == "nt" else "receive")
        c_file.write_text(source, encoding="utf-8")
        subprocess.run([os.environ.get("CC", "gcc"), "-std=c11", "-Wall", "-Wextra", "-Werror", str(c_file), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
