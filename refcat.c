/* refcat - cat multiple files into a single output file, using reflinks
 *
 * ISC License
 *
 * Copyright (c) 2019, Steve Gilberd <steve@erayd.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * ================================== refcat ===================================
 *
 * Please note that *output* alignment does matter. Your filesystem may not
 * support using reflinks in the output file at non-aligned offsets. Data which
 * cannot be linked will instead be copied in full, which ensures completeness
 * of the output file, but may result in higher than expected disk space
 * usage.
 *
 * Your filesystem may also not support reflinks for very small extents - in
 * such a case, the source data will be copied in full.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <error.h>
#include <errno.h>
#include <linux/limits.h>
#include <linux/fs.h>


int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "No target specified\n");
        return 1;
    }
    int target = open(argv[1], O_CREAT | O_WRONLY, 0644);

    char part[PATH_MAX];
    while (fgets((char * restrict)&part, sizeof(part), stdin)) {
        strtok(part, "\n");
        if (part[0] == '\n') {
            continue;
        }
        struct file_clone_range range = {
            .src_fd = open(part, O_RDWR),
            .src_offset = 0,
            .src_length = 0,
            .dest_offset = lseek(target, 0, SEEK_END)
        };
        if (range.src_fd < 0) {
            error(errno, errno, "Failed opening source (%s)", part);
        }
        if (ioctl(target, FICLONERANGE, &range)) {
            ssize_t remaining = lseek(range.src_fd, 0, SEEK_END);
            lseek(range.src_fd, 0, SEEK_SET);
            while (remaining) {
                remaining -= copy_file_range(
                    range.src_fd,
                    NULL,
                    target,
                    NULL,
                    remaining,
                    0
                );
                if (remaining < 0) {
                    error(errno, errno, "Fallback copy failed (%s)", part);
                }
            }
        }
        close(range.src_fd);
    }

    close(target);
    return 0;
}
