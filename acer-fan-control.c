#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kthread.h>  // for threads
#include <linux/time.h>
#include <linux/timer.h>

#define EC_VAL 0x62
#define EC_CMD 0x66

#ifndef SLEEP_MILLI_SEC
#define SLEEP_MILLI_SEC(nMilliSec)\
do { \
long timeout = (nMilliSec) * HZ / 1000; \
while(timeout > 0) \
{ \
timeout = schedule_timeout(timeout); \
} \
}while(0);
#endif

static struct task_struct *thread1;
static int fanSpeed;
static int maxFanSpeed = 130;
static int minFanSpeed = 50;

static void wait_write_ec(void)
{
  while((inb(EC_CMD) & 0x02) != 0) {

  }
}

static void wait_read_ec(void)
{
  while ((inb(EC_CMD) & 0x01) == 0) {

  }
}

static void write_ec(short addr, short value)
{
  wait_write_ec();
  outb(0x81,EC_CMD);
  wait_write_ec();
  outb(addr,EC_VAL);
  wait_write_ec();
  outb(value,EC_VAL);
}


static int read_ec(short addr)
{
  wait_write_ec();
  outb(0x80,EC_CMD);
  wait_write_ec();
  outb(addr,EC_VAL);
  wait_read_ec();
  return inb(EC_VAL);
}

static void set_speed(short speed)
{
  write_ec(0x94, (255-(speed * 135) / 100));
}

static int get_max(short cpu, short gpu)
{
  int retval = 0;
  if (cpu > gpu) {
    retval = cpu;
  } else {
    retval = gpu;
  }

  return retval;
}

int thread_fn(void *data)
{
  short cpu_temp,gpu_temp,max_temp;
  set_current_state(TASK_INTERRUPTIBLE);
  while(!kthread_should_stop())
  {
    SLEEP_MILLI_SEC(30000);
    cpu_temp = 0;
    gpu_temp = 0;
    cpu_temp = read_ec(0xA8);
    gpu_temp = read_ec(0xAF);
    max_temp = get_max(cpu_temp, gpu_temp);
    if (max_temp < 60) {
      fanSpeed = minFanSpeed;
    }

    if (max_temp > 60) {
      fanSpeed = minFanSpeed+20;
    }

    if (max_temp > 70) {
      fanSpeed = minFanSpeed+40;
    }

    if (max_temp > 80) {
      fanSpeed = maxFanSpeed;
    }
    set_speed(fanSpeed);
    printk(KERN_INFO "cpu temp: %d, gpu temp: %d, fan speed: %d", cpu_temp, gpu_temp, fanSpeed);
    schedule();
    set_current_state(TASK_INTERRUPTIBLE);
  }
  set_current_state(TASK_RUNNING);
  return 0;
}

static int __init init_fan_module(void)
{
  char name[16]="acer_fan_control";
  printk(KERN_CRIT "Fan control started\n");
  // turn BIOS fan control off  
  write_ec(0x93, 0x14);

  printk(KERN_INFO "in init");
  thread1 = kthread_run(thread_fn,NULL,name);
  
  return 0;
}

static void __exit fan_module_exit(void)
{
  
  int ret;
  ret = kthread_stop(thread1);
  if(!ret)
    printk(KERN_INFO "Thread stopped");
  printk(KERN_CRIT "Fan control cleanup\n");
}

module_init(init_fan_module);
module_exit(fan_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jason Miesionczek");
MODULE_DESCRIPTION("Control fan speed for Acer 7551g Notebooks");
