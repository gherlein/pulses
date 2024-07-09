#include "hardware/structs/iobank0.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include <stdio.h>

#define GPIO_TRIGGER_OUT 18
#define GPIO_FAKE_OUT 19
#define GPIO_TRIGGER_IN 20
#define GPIO_MEASURE_IN 21

uint32_t gpio_get_events(uint gpio) {
  int32_t mask = 0xF << 4 * (gpio % 8);
  return (iobank0_hw->intr[gpio / 8] & mask) >> 4 * (gpio % 8);
}
void gpio_clear_events(uint gpio, uint32_t events) {
  gpio_acknowledge_irq(gpio, events);
}

#define FLAG_VALUE 123

int64_t trigger_fall(alarm_id_t id, void *user_data) {
  // printf("Timer %d fired!\n", (int)id);
  gpio_put(GPIO_TRIGGER_OUT, 0);
  return 0;
}

int64_t pulse_follow_rise(alarm_id_t id, void *user_data) {
  // printf("Timer %d fired!\n", (int)id);
  gpio_put(GPIO_FAKE_OUT, 1);
  return 0;
}
int64_t pulse_follow_fall(alarm_id_t id, void *user_data) {
  gpio_put(GPIO_FAKE_OUT, 0);
  return 0;
}

bool trigger_rise(struct repeating_timer *t) {
  printf("Repeat at %lld\n", time_us_64());
  add_alarm_in_ms(10, trigger_fall, NULL, true);
  add_alarm_in_ms(100, pulse_follow_rise, NULL, true);
  add_alarm_in_ms(110, pulse_follow_fall, NULL, true);
  gpio_put(GPIO_TRIGGER_OUT, 1);
  return true;
}

void core1_entry() {
  struct repeating_timer timer;

  multicore_fifo_push_blocking(FLAG_VALUE);
  uint32_t g = multicore_fifo_pop_blocking();

  if (g != FLAG_VALUE)
    printf("Hmm, that's not right on core 1!\n");
  else
    printf("Setting up calibration pulses on core 1\n");

  gpio_set_function(GPIO_TRIGGER_OUT, GPIO_FUNC_SIO);
  gpio_set_dir(GPIO_TRIGGER_OUT, true);
  gpio_set_function(GPIO_FAKE_OUT, GPIO_FUNC_SIO);
  gpio_set_dir(GPIO_FAKE_OUT, true);
  add_repeating_timer_ms(1000, trigger_rise, NULL, &timer);
}

int main() {
  uint64_t tStart = 0;
  uint64_t tEnd = 0;
  uint64_t tElapsed = 0;

  stdio_init_all();

  multicore_launch_core1(core1_entry);
  uint32_t g = multicore_fifo_pop_blocking();

  if (g != FLAG_VALUE)
    printf("Hmm, that's not right on core 0!\n");
  else {
    multicore_fifo_push_blocking(FLAG_VALUE);
    printf("Measuring pulse delta on core 0\n");
  }

  gpio_set_function(GPIO_TRIGGER_IN, GPIO_FUNC_SIO);
  gpio_set_dir(GPIO_TRIGGER_IN, false);
  gpio_pull_down(GPIO_TRIGGER_IN);
  gpio_set_function(GPIO_MEASURE_IN, GPIO_FUNC_SIO);
  gpio_set_dir(GPIO_MEASURE_IN, false);
  gpio_pull_down(GPIO_MEASURE_IN);

  while (true) {

    gpio_clear_events(GPIO_TRIGGER_IN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL);
    while (!(gpio_get_events(GPIO_TRIGGER_IN) & GPIO_IRQ_EDGE_RISE)) {
    };
    tStart = time_us_64();

    gpio_clear_events(GPIO_MEASURE_IN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL);
    while (!(gpio_get_events(GPIO_MEASURE_IN) & GPIO_IRQ_EDGE_RISE)) {
    };
    tEnd = time_us_64();
    tElapsed = (tEnd - tStart);
    printf("latency: %llu\n", tElapsed);
  }
}
