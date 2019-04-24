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

#define PROGRAM_NAME "pdd"
#define TRUE  1
#define FALSE 0
#define ONE_GB (1024 * 1024 * 1024)
#define ONE_MB (1024 * 1024)
#define ONE_KB (1024)
#define LEN_SM_BUF 300
#define LEN_MD_BUF 500
#define LEN_LG_BUF 1000
#define DEFAULT_BS 4096          /* default block size */
#define CURRENT_YEAR "2019"
#define VERSION "1.0.0"

struct th_info {
    int       thread_num;
    pthread_t thread_id;
    char      *dev_src;
    char      *dev_dst;
    off_t     skip;
    off_t     seek;
    size_t    size_data;
    size_t    size_buff;
};
