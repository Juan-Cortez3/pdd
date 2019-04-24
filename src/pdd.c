/*
   pdd -- a parallel version GNU/Linux native dd command
   Copyright (C) 2019  Juan Cortez

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/


#define _GNU_SOURCE

#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <regex.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <sched.h>
#include <getopt.h>
#include "pdd.h"

static char dev_src[LEN_SM_BUF];
static char dev_dst[LEN_SM_BUF];
static off_t off_src;
static off_t off_dst;
static size_t size_count;
static size_t size_block;
static uint8_t cnt_thread;             /* count of threads used */
static uint8_t flag_verbose;
static char flag_direct;               /* The direct io flag tries to minimize cache effects */


void handle_error(char *msg);
void handle_error_en(int en, char *msg);
void handle_reg_error(int ret, regex_t *addr_reg);
void print_usage(void);
void scan_args(int arg, char *argv[]);
void* trans_data(void *arg);
void init_global_var(void);
size_t parse_int_with_unit(regex_t *ptr_reg, char *ptr_input);

char short_opt[] = "hi:o:s:e:b:c:d:t:v";
struct option long_options[] = {
     {"verbose", no_argument,       &flag_verbose, 1},
     {"version", no_argument,       0, 'v'},
     {"help",    no_argument,       0, 'h'},
     {"if",      required_argument, 0, 'i'},
     {"of",      required_argument, 0, 'o'},
     {"skip",    required_argument, 0, 's'},
     {"seek",    required_argument, 0, 'e'},
     {"bs",      required_argument, 0, 'b'},
     {"count",   required_argument, 0, 'c'},
     {"direct",  required_argument, 0, 'd'},
     {"threads", required_argument, 0, 't'},
     {0,         0,                 0, 0}
};

int
main(int argc, char *argv[])
{
    void *res;

    setlinebuf(stdout);
    init_global_var();
    scan_args(argc, argv);
    verify_global_var();
    /* The actual offset values and data size value is multiplicated by block size */
    size_t size_data = size_count * size_block;
    off_src *= size_block;
    off_dst *= size_block;

    struct timeval time_start, time_end;
    double duration_ms;

    gettimeofday(&time_start, NULL);

    /* allocate memory for pthread_create() arguments */
    struct th_info* tinfo = calloc(cnt_thread, sizeof(struct th_info));

    size_t size_th_data = size_data / cnt_thread;
    int ret = 0;
    uint8_t i = 0;
    for (i = 0; i < cnt_thread; i++) {
        tinfo[i].thread_num = i;
        tinfo[i].dev_src = dev_src;
        tinfo[i].dev_dst = dev_dst;
        tinfo[i].skip = off_src + i * size_th_data;
        tinfo[i].seek = off_dst + i * size_th_data;
        tinfo[i].size_data = size_th_data;
        tinfo[i].size_buff = size_block;

        ret = pthread_create(&tinfo[i].thread_id, NULL, trans_data, &tinfo[i]);
        if (0 != ret) {
            handle_error_en(ret, "pthread_create");
            return 1;
        }
    }

    for (i = 0; i < cnt_thread; i++) {
        ret = pthread_join(tinfo[i].thread_id, &res);
        if (0 != ret) {
            handle_error_en(ret, "pthread_join");
            return 1;
        }
    }

    gettimeofday(&time_end, NULL);
    duration_ms = ((time_end.tv_sec - time_start.tv_sec) * 1000000 + time_end.tv_usec - time_start.tv_usec) / 1000;

    char unit_display_total = '\0';
    char unit_display_average = '\0';
    double size_data_display_total = size_data;
    double size_data_display_average = size_data / duration_ms * 1000;
    if (size_data >= ONE_GB) {
        unit_display_total = 'G';
        size_data_display_total /= ONE_GB;
    } else if (size_data >= ONE_MB) {
        unit_display_total = 'M';
        size_data_display_total /= ONE_MB;
    } else if (size_data >= ONE_KB) {
        unit_display_total = 'K';
        size_data_display_total /= ONE_KB;
    } else {
        unit_display_total = 'B';
    }
    if (size_data_display_average >= ONE_GB) {
        unit_display_average = 'G';
        size_data_display_average /= ONE_GB;
    } else if (size_data_display_average >= ONE_MB) {
        unit_display_average = 'M';
        size_data_display_average /= ONE_MB;
    } else if (size_data_display_average >= ONE_KB) {
        unit_display_average = 'K';
        size_data_display_average /= ONE_KB;
    } else {
        unit_display_average = 'B';
    }

    printf("  %6.2f %c copied, %7.3f s, %7.3f %c/s\n", size_data_display_total, unit_display_total, duration_ms / 1000, size_data_display_average, unit_display_average);
    return 0;
}

/*
 * Transfer data
 *
 */
void*
trans_data(void *arg)
{
    struct th_info* tinfo = arg;
    int fd_src = 0;
    int fd_dst = 0;

    cpu_set_t mask_cpu;
    CPU_ZERO(&mask_cpu);
    CPU_SET(tinfo->thread_num, &mask_cpu);

    sched_setaffinity(0, sizeof(mask_cpu), &mask_cpu);
    /* open target file */
    fd_src = open(tinfo->dev_src, O_RDWR | O_SYNC | ('i' == flag_direct ? O_DIRECT : 0));
    if (-1 == fd_src) {
        handle_error("open");
        close(fd_src);
        return (void *)1;
    }

    fd_dst = open(tinfo->dev_dst, O_RDWR | O_SYNC | ('o' == flag_direct ? O_DIRECT : 0));
    if (-1 == fd_dst) {
        handle_error("open");
        close(fd_dst);
        return (void *)1;
    }

    /* reposition the file offsets of the opened files */
    if (-1 == lseek(fd_src, tinfo->skip, SEEK_SET)) {
        handle_error("lseek");
        close(fd_src);
        return (void *)1;
    }

    if (-1 == lseek(fd_dst, tinfo->seek, SEEK_SET)) {
        handle_error("lseek");
        close(fd_dst);
        return (void *)1;
    }

    void* p_buffer;
    if (0 != posix_memalign(&p_buffer, (size_t)sysconf(_SC_PAGESIZE), tinfo->size_buff)) {
        printf("Failed to allocate space for buffer!\n");
        close(fd_src);
        close(fd_dst);
        return (void *)1;
    }

    posix_madvise(p_buffer, tinfo->size_buff, POSIX_MADV_SEQUENTIAL);
    size_t cnt = tinfo->size_data / tinfo->size_buff;
    size_t ind = 0;
    for (ind = 0; ind < cnt; ind++) {
        /* read from the source file */
        if (tinfo->size_buff != read(fd_src, p_buffer, tinfo->size_buff)) {
            handle_error("read");
            close(fd_src);
            return (void *)1;
        }

        /* write to the target file */
        if (tinfo->size_buff != write(fd_dst, p_buffer, tinfo->size_buff)) {
            handle_error("write");
            close(fd_dst);
            return (void *)1;
        }
    }

    /* close the file descriptor */
    if (-1 == close(fd_src)) {
        handle_error("close");
        return (void *)1;
    }

    if (-1 == close(fd_dst)) {
        handle_error("close");
        return (void *)1;
    }

    free(p_buffer);
    return (void *)0;
}


void
init_global_var(void)
{
    optind      = 1;
    dev_src[0]  = '\0';
    dev_dst[0]  = '\0';
    off_src     = 0;
    off_dst     = 0;
    size_count  = 0;
    size_block  = 0;
    flag_direct = '\0';
    cnt_thread  = 1;
}


void
verify_global_var(void)
{
    /* verify if the arguments are correctly set */
    if ('\0' == dev_src[0])
        handle_error_en(ENOENT, "source file null");

    if (-1 == access(dev_src, F_OK))
        handle_error("source file does not exist");

    if (-1 == access(dev_src, R_OK))
        handle_error("source file is not readable");

    if ('\0' == dev_dst[0])
        handle_error_en(ENOENT, "destination file null");

    if (-1 == access(dev_dst, F_OK))
        handle_error("destination file does not exist");

    if (-1 == access(dev_dst, W_OK))
        handle_error("destination file is not writable");

    if (0 == size_count)
        handle_error_en(ENOENT, "trans count zero");

    if (0 == size_block)
        size_block = DEFAULT_BS;

    if (cnt_thread > get_nprocs()) {
        char err_msg[LEN_SM_BUF];
        int max_th_cnt = get_nprocs();
        sscanf(err_msg, "the count of thread is too big, exceeds max thread count: %d", &max_th_cnt);
        handle_error_en(EINVAL, err_msg);
    }
}


void
scan_args(int argc, char *argv[])
{
    int c_opt, option_index;
    char **ptr_tail = NULL;           /* used in function strtoull */
    unsigned long long int arg_num = 0; /* used to contain numbers parsed from args  */
    /* use regular expression to deal with the input numerical parameters */
    regex_t reg;
    char ptrn_reg[] = "[[:digit:]]+([KMG])?";
    int ret_regcomp = regcomp(&reg, ptrn_reg, REG_EXTENDED);
    if (0 != ret_regcomp) {
        printf("cannot compile regex: %s\n", ptrn_reg);
        handle_reg_error(ret_regcomp, &reg);
    }
    while (-1 != (c_opt = getopt_long(argc, argv, short_opt, long_options, option_index))) {
        arg_num = 0;
        ptr_tail = 0;
        switch(c_opt) {
        case 0:
            break;
        case 'v':
            printf("current version: " VERSION "\n");
            return 0;
        case 'i':
            if (LEN_SM_BUF > strlen(optarg)) {
                strncpy(dev_src, optarg, strlen(optarg));
            } else {
                handle_error_en(E2BIG, "argument for source file is too long");
            }
            break;
        case 'o':
            if (LEN_SM_BUF > strlen(optarg)) {
                strncpy(dev_dst, optarg, strlen(optarg));
            } else {
                handle_error_en(E2BIG, "argument for destination file is too long");
            }
            break;
        case 's':
            off_src = (off_t)parse_int_with_unit(&reg, optarg);
            break;
        case 'e':
            off_dst = (off_t)parse_int_with_unit(&reg, optarg);
            break;
        case 'c':
            size_count = (size_t)parse_int_with_unit(&reg, optarg);
            break;
        case 'b':
            size_block = (size_t)parse_int_with_unit(&reg, optarg);
            break;
        case 'd':
            flag_direct = optarg[0];
            if ('i' != flag_direct && 'o' != flag_direct) {
                fprintf(stderr, "Invalid direct flag!\n");
            }
            if (flag_verbose)
                printf("direct flag: %c\n", flag_direct);
            break;
        case 't':
            errno = 0;
            arg_num = strtoull(optarg, ptr_tail, 10);
            if (ERANGE == errno)
                handle_error_en(ERANGE, "argument for threads is out of range.");
            if (arg_num <= 255) {
                cnt_thread = (uint8_t)arg_num;
            } else {
                handle_error_en(E2BIG, "argument for threads is too long");
            }
            if (flag_verbose)
                printf("Thread count: %d\n", cnt_thread);
            break;
        case 'h':
            print_usage();
            exit(0);
        case '?':
            return;
        default:
            abort();
        }
    }

    regfree(&reg);
}

void
print_usage(void)
{
    printf("Usage: " PROGRAM_NAME " OPTION\n"
           "read from the source file and write to the target file\n"
           "\t--if       path to the source file\n"
           "\t--of       path to the target file\n"
           "\t--skip     skip xx[KMG] data at the beginning of source file \n"
           "\t--seek     skip xx[KMG] data at the beginning of destination file\n"
           "\t--bs       read and write xx[KMG] bytes data at a time\n"
           "\t--count    the count of blocks will be processed in total\n"
           "\t--direct   use direct io, i for input and o for output\n"
           "\t--threads  thread count, set fixed thread count\n");
}

size_t
parse_int_with_unit(regex_t *ptr_reg, char *ptr_input)
{
    int ret_regexec = 0;
    regmatch_t ptr_match[1];
    char str_temp[LEN_SM_BUF];
    char unit = '\0';
    size_t len_str = 0;
    char **ptr_tail = NULL;           /* used in function strtoull */
    size_t result = 1;

    memset(str_temp, 0, LEN_SM_BUF);
    unsigned long long int arg_num = 0; /* used to contain numbers parsed from args  */

    ret_regexec = regexec(ptr_reg, ptr_input, 1, ptr_match, 0);
    if (0 != ret_regexec) {
        printf("regex match failed to match %s\n", ptr_input);
        handle_reg_error(ret_regexec, ptr_reg);
    }

    /* get the numerical value */
    len_str = ptr_match[0].rm_eo - ptr_match[0].rm_so;
    if (LEN_SM_BUF > len_str) {
        strncpy(str_temp, optarg + ptr_match[0].rm_so, len_str);
    } else {
        handle_error_en(E2BIG, "argument for threads is too long");
    }

    /* check if the input has a unit */
    unit = optarg[ptr_match[0].rm_eo - 1];
    if (unit < '0' | unit > '9') {
        switch(unit) {
        case 'K':
            result = 1024;
            break;
        case 'M':
            result = 1024 * 1024;
            break;
        case 'G':
            result = 1024 * 1024 * 1024;
            break;
        default:
            fprintf(stderr, "unknown unit character: %c\n", unit);
            exit(EXIT_FAILURE);
        }
    }
    errno = 0;
    arg_num = strtoull(str_temp, ptr_tail, 10);
    if (ERANGE == errno)
        handle_error_en(ERANGE, "input parameter is out of range.");

    return result * arg_num;
}


void
handle_error(char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}


void
handle_error_en(int en, char *msg)
{
    errno = en;
    perror(msg);
    exit(EXIT_FAILURE);
}


void
handle_reg_error(int ret, regex_t *addr_reg)
{
    char errbuf_reg[LEN_SM_BUF];
    regerror(ret, addr_reg, errbuf_reg, LEN_SM_BUF);
    fprintf(stderr, "%s\n", errbuf_reg);
    exit(EXIT_FAILURE);
}
