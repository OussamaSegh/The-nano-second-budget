## Appendix B. Setting Up a Low-Noise Lab Machine {#appB}

Chapter 17 explains *why* each setting matters. This appendix is the condensed procedure, for a dedicated Linux machine on which you want benchmark results that reflect your code rather than the platform. Apply it on a machine you control and can reboot; several settings trade power efficiency and general-purpose performance for predictability, and none of them should be applied to a shared server without agreement.

The examples assume a single-socket machine with 8 physical cores (logical CPUs 0 to 15, with CPU *n* and *n* + 8 as SMT siblings), using physical cores 4 to 7 for benchmarks and 0 to 3 for everything else. Adjust the numbers to your topology: `lscpu -e` shows it.

### 1. Hardware and firmware

- In the BIOS or UEFI setup, set the power profile to maximum performance; disable deep package C-states if the option exists; consider disabling SMT (or plan to keep the siblings of benchmark cores idle); disable turbo if you want run-to-run consistency more than peak speed.
- Check for system management interrupts with `hwlatdetect --duration=60` (from the `rt-tests` package). Anything above a few microseconds points at firmware settings.

### 2. Kernel command line

Edit `/etc/default/grub` (or your boot loader's equivalent), add the following to the kernel command line, regenerate the configuration (`update-grub` or `grub2-mkconfig -o ...`) and reboot:

```text
isolcpus=managed_irq,domain,4-7,12-15 nohz_full=4-7,12-15 rcu_nocbs=4-7,12-15
irqaffinity=0-3,8-11 intel_idle.max_cstate=1 processor.max_cstate=1
transparent_hugepage=madvise nosoftlockup
```

Including the SMT siblings (12 to 15) in the isolated set keeps them idle. Use `nosmt` instead if you disabled SMT in software rather than firmware. Verify after the reboot:

```text
cat /sys/devices/system/cpu/isolated        # 4-7,12-15
cat /sys/devices/system/cpu/nohz_full       # 4-7,12-15
```

### 3. Run-time settings

Run as root after every boot (or put them in a systemd unit):

```text
# CPU frequency: performance governor on all cores
cpupower frequency-set -g performance

# Interrupts: stop irqbalance and move every IRQ to the housekeeping cores
systemctl stop irqbalance
for f in /proc/irq/*/smp_affinity_list; do echo 0-3,8-11 > "$f" 2>/dev/null; done

# Unbound kernel workqueues on housekeeping cores only (hex mask of CPUs 0-3, 8-11)
echo f0f > /sys/devices/virtual/workqueue/cpumask

# Memory
swapoff -a
sysctl -w vm.swappiness=0
sysctl -w kernel.numa_balancing=0
echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
echo 512 > /proc/sys/vm/nr_hugepages        # 1 GiB of explicit 2 MiB huge pages

# Scheduler: no RT throttling on a dedicated machine (busy-spinning RT threads
# can then starve housekeeping on their core, so only use RT on isolated cores)
sysctl -w kernel.sched_rt_runtime_us=-1
```

Alternatively, the `tuned` daemon's `latency-performance` or `network-latency` profiles apply most of the run-time settings, and its `cpu-partitioning` profile handles isolation.

### 4. Verify the quiet

```text
# Interrupt counts on the isolated cores should barely change over 10 seconds
grep -E 'CPU|LOC|RES|CAL|TLB' /proc/interrupts > /tmp/a; sleep 10
grep -E 'CPU|LOC|RES|CAL|TLB' /proc/interrupts > /tmp/b; diff /tmp/a /tmp/b

# No threads other than per-CPU kernel threads on cores 4-7
ps -eLo psr,comm | awk '$1>=4 && $1<=7' | sort | uniq -c

# Platform jitter on an isolated core, from the companion code (Chapter 1)
./build/ch01_jitter --cpus=5,0
```

On a well-isolated core, `ch01_jitter` should show a p99.99 of a few hundred nanoseconds and a maximum in the low microseconds. If the maximum is in the tens of microseconds, check `rtla osnoise top -c 5` (or `perf trace` and `ftrace`) to see what is still interrupting the core.

### 5. Run benchmarks

- Pin each benchmark thread to an isolated core, using the examples' `--cpus=A,B` option or `taskset -c`.
- Put cooperating threads on different physical cores of the same socket (and the same L3 slice group, on processors that have them), unless the experiment is about SMT or NUMA.
- Run each benchmark several times and report the range, not only the best run (Chapter 4).
- Record the environment: the companion examples print the CPU model, kernel and governor at the top of their output; add the kernel command line (`cat /proc/cmdline`) to your notes.

### 6. Undo

Remove the kernel parameters and reboot; restart `irqbalance`; set the governor back to `schedutil` or `powersave`; re-enable swap. The run-time settings do not survive a reboot unless you made them persistent.

### Cloud machines

On a cloud VM, most of the above is unavailable or ineffective, since the hypervisor owns the physical cores. To get closer to lab conditions in the cloud, use a bare-metal instance type (where the whole procedure applies) or a dedicated-host instance with pinned virtual CPUs; check steal time (`st` in `top`, or `vmstat 1`) during every run; and report the instance type with your results, as this book does for its reference machine.
