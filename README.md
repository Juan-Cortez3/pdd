## pdd

### Introduction
---
pdd is the parallel version of the GNU/Linux native tool **dd**. It provides better IO performance on NVMe drives.
pdd spawns several threads for the data transportation job. Each thread is bond to one CPU core to achieve best efficiency.

### Installation
---
#### 1. For user
Just download the latest release tar package, and:
```shell
./configure && make %% sudo make install
```

#### 2. For software contributor
clone this repo and:
```shell
./autogen.sh
./configure # Generate Makefile from Makefile.am
make
sudo make install
```

### Usage
---
#### 1. Basics
To use this tool, at least 3 parameters must be supplied:
* source device file
* destination device file
* the count of blocks need to be transported

for example:
```shell
pdd --if /dev/zero --of /dev/null --count 256
```

by default, the **block size** is set to 4K Bytes, the actual data size transported is: count * bs.
Here, the total size of data involved is 256 * 4K = 1M.

#### 2. Offsets
parameters **skip** and **seek** specify the offsets in source file and destination files. Their values are based
on **bs** size as well:
```shell
pdd --if /dev/zero --of /dev/null --bs 1M --count 256 --skip 128 --seek 512
```
pdd read from `/dev/zero` device with an offset of 128M and write to `/dev/null` with an offset of 512M. Data transported is
also 256M.

Note here we can feed values to **bs**, **count**, **skip** and **seek** in this pattern: `[[:digit:]]+([KMG])?`
the unit `KMG` can be omitted.

#### 3. Threads & direct flag 
You can specify the count of threads by: `--threads 8`.
Note that the thread count cannot exceed the count of the logical cores of your CPU.

You can also specify `--direct i/o` to notify pdd to open the source or destination file with **direct access mode**.
This will minimize cache effects of the I/O to and from this file. In other words, this flag makes the data transportation
synchronously.

But if you use this flag on files wihch do not support direct access mode, pdd will fail.

### Reporting bugs or suggestions
---
It is likely that you will encounter bugs in **pdd** or if you want more functionalities. I would like to hear about it. As the
purpose of bug reporting is to improve software, please be sure to include maximum information when reporting a bug. The information
needed is:
* Version of the package you are using
* Compilation options used when configuring the package.
* Conditions under which the bug appears.

Send your report to `juancortez0128@gmail.com`. Allow me a couple of days to answer.