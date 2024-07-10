#include "hardware/structs/iobank0.h"
#include "lwip/apps/httpd.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GPIO_TRIGGER_OUT 18
#define GPIO_FAKE_OUT 19
#define GPIO_TRIGGER_IN 20
#define GPIO_MEASURE_IN 21
#define FLAG_VALUE 123

char cycle = 0;
char counter = 0;
char delta = 90;
uint64_t tStart = 0;
uint64_t tEnd = 0;
uint64_t tElapsed = 0;

char ssid[] = "sofia";
char pass[] = "19631964";
uint32_t country = CYW43_COUNTRY_USA;
uint32_t auth = CYW43_AUTH_WPA2_MIXED_PSK;

int setup(uint32_t country, const char *ssid, const char *pass, uint32_t auth,
          const char *hostname, ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw) {

  if (cyw43_arch_init_with_country(country)) {
    return 1;
  }

  cyw43_arch_enable_sta_mode();
  if (hostname != NULL) {
    netif_set_hostname(netif_default, hostname);
  }
  if (cyw43_arch_wifi_connect_async(ssid, pass, auth)) {
    return 2;
  }
  int flashrate = 1000;
  int status = CYW43_LINK_UP + 1;
  while (status >= 0 && status != CYW43_LINK_UP) {
    int new_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (new_status != status) {
      status = new_status;
      flashrate = flashrate / (status + 1);
      printf("connect status: %d %d\n", status, flashrate);
    }
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    sleep_ms(flashrate);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    sleep_ms(flashrate);
  }
  if (status < 0) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
  } else {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    if (ip != NULL) {
      netif_set_ipaddr(netif_default, ip);
    }
    if (mask != NULL) {
      netif_set_netmask(netif_default, mask);
    }
    if (gw != NULL) {
      netif_set_gw(netif_default, gw);
    }

    printf("IP: %s\n", ip4addr_ntoa(netif_ip_addr4(netif_default)));
    printf("Mask: %s\n", ip4addr_ntoa(netif_ip_netmask4(netif_default)));
    printf("Gateway: %s\n", ip4addr_ntoa(netif_ip_gw4(netif_default)));
    printf("Host Name: %s\n", netif_get_hostname(netif_default));
  }
  return status;
}

/****************************************** EVENTS ***************************/

uint32_t gpio_get_events(uint gpio) {
  int32_t mask = 0xF << 4 * (gpio % 8);
  return (iobank0_hw->intr[gpio / 8] & mask) >> 4 * (gpio % 8);
}
void gpio_clear_events(uint gpio, uint32_t events) {
  gpio_acknowledge_irq(gpio, events);
}

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

  //  printf("Repeat at %lld\n", time_us_64());
  if (counter == 10) {
    cycle++;
    counter = 0;
  }
  switch (cycle) {
  case 0:
    delta = 90;
    break;
  case 1:
    delta = 100;
    break;
  case 2:
    delta = 110;
    break;
  default:
    cycle = 0;
    break;
  }

  add_alarm_in_ms(10, trigger_fall, NULL, true);
  add_alarm_in_ms(delta, pulse_follow_rise, NULL, true);
  add_alarm_in_ms(delta + 10, pulse_follow_fall, NULL, true);
  gpio_put(GPIO_TRIGGER_OUT, 1);
  counter++;
  return true;
}

void core1_entry() {
  struct repeating_timer timer;

  multicore_fifo_push_blocking(FLAG_VALUE);
  uint32_t g = multicore_fifo_pop_blocking();

  if (g != FLAG_VALUE)
    printf("Hmm, that's not right on core 1!\n");
  else
    printf("Setting up GPIO on core 1\n");

  gpio_set_function(GPIO_TRIGGER_OUT, GPIO_FUNC_SIO);
  gpio_set_dir(GPIO_TRIGGER_OUT, true);
  gpio_set_function(GPIO_FAKE_OUT, GPIO_FUNC_SIO);
  gpio_set_dir(GPIO_FAKE_OUT, true);
  add_repeating_timer_ms(1000, trigger_rise, NULL, &timer);

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

int main() {

  stdio_init_all();

  multicore_launch_core1(core1_entry);
  uint32_t g = multicore_fifo_pop_blocking();

  if (g != FLAG_VALUE)
    printf("Hmm, that's not right on core 0!\n");
  else {
    multicore_fifo_push_blocking(FLAG_VALUE);
    printf("Running server on core 0\n");
  }

#define ENABLE_NET
#ifdef ENABLE_NET
  setup(country, ssid, pass, auth, "pulses", NULL, NULL, NULL);
  httpd_init();

#endif
  return 0;
}
