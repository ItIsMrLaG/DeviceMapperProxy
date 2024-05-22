# DeviceMapperProxy (dmp)
## Description 
> **dmp** is a kernel module for `linux` that creates virtual block devices on top of an existing device mapper-based device and monitors the statistics of operations performed on the device. The statistics are accessible through the `sysfs` module.
 
## Instalation

To install `dmp` module to your system use `make` script to build kernel module and `insmod` command to insert it:

```bash
make
insmod dmp.ko
```
> Check the availability of `dmp` module by command `dmsetup targets`
 

## Usage

### Virtual device creation
To create virtual device using `dmp`:
```bash
sudo dmsetup create $virtual_dev_name --table "0 $num_sectors dmp /dev/$some_dev"
```
> Where: 
> * `$virtual_dev_name` - name of a new virtual device (*e.g. dmp1*)
> * `$num_sectors` - size of a new virtual device in sectors
> * `$some_dev` - underlying device

> For more information see [dmsetup manual](https://man7.org/linux/man-pages/man8/dmsetup.8.html).

### Statistics

Using `sysfs`, can be obtained the following statistics about created virtual devices:

* `read_cnt` - number of read requests
* `write_cnt` - number of write requests
* `total_cnt` - total number of requests
* `read_avr_bs` - average block size per read
* `write_avr_bs` - average block size per write
* `total_avr_bs` - total average block size

These userspace statistics are accessible via `sysfs` attributes:
```bash
cat /sys/module/dmp/stat/$device_name/$attribute
```
Where:

* `$attribute` - some statistical information
* `$device_name` - virtual device kernel name

> Summary statistics are also available in formatted form:
> ```bash
> cat /sys/module/dmp/stat/$device_name/summary
> ```
> Output:
> ```
> read:
>        reqs: <number>
>        avg size: <number>
>write:
>        reqs: <number>
>        avg size: <number>
>total:
>        reqs: <number>
>        avg size: <number>
> ```

All statistics are available both for each device and for all devices simultaneously. To access common statistics see `/sys/module/dmp/stat/all_devs/`

> Spinlocks are used to maintain consistency of statistical data.

 

## Testing

> Tests were carried out on the kernel: `Linux 6.2.0-39-generic x86_64`

Example test script (with `dd`):
```bash
sudo dmsetup create zero1 --table "0 $size zero" 
sudo dmsetup create dmp1 --table "0 $size dmp /dev/mapper/zero1"
sudo dd if=/dev/random of=/dev/mapper/dmp1 bs=4k count=1
cat /sys/module/dmp/stat/all_devs/summary

```
This script tests writing to the device. First, `dmp1` is created based on the virtual block device `zero1`. Then a record is made to it through the `dd` utility. At the very end, statistics are displayed.

> Strongly recomended insted of `dd` using . 
> It is strongly recommended to use [`fio`](https://fio.readthedocs.io/en/latest/fio_doc.html) utility instead of `dd`.
 

## License
Distributed under the GPLv2 License. See [LICENSE](LICENSE) for more information.