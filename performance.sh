#!/bin/sh
#This script adjust some system settings to avoid interference while analyzing the performance of fibdrv.
#2022/04/19


CPUID=1
ORIG_ASLR=`cat /proc/sys/kernel/randomize_va_space`
ORIG_GOV=`cat /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_governor`
ORIG_TURBO=`cat /sys/devices/system/cpu/intel_pstate/no_turbo`

# adjust system setting
sudo sh -c "echo 0 > /proc/sys/kernel/randomize_va_space"
sudo sh -c "echo performance > /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_governor"
sudo sh -c "echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo"

# analyze the performance
make client_m
make unload
make load
sudo taskset -c 1 ./client_m
gnuplot plot.gp
make unload

# restore system setting
sudo sh -c "echo $ORIG_ASLR > /proc/sys/kernel/randomize_va_space"
sudo sh -c "echo $ORIG_GOV > //sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_governor"
sudo sh -c "echo $ORIG_TURBO > /sys/devices/system/cpu/intel_pstate/no_turbo"