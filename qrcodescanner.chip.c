// Wokwi Custom Chip - For docs and examples see:
// https://docs.wokwi.com/chips-api/getting-started
//
// SPDX-License-Identifier: MIT
// Copyright 2023 
//test fork

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  // TODO: Put your chip variables here
  pin_t inpin;
  uart_dev_t uart0;
  uint32_t tupnum_attr;
} chip_state_t;


void input_changed(void *user_data, pin_t pin, uint32_t value) {
  // value will either be HIGH or LOW
  chip_state_t* chip = (chip_state_t*)user_data;
  //ipasa mo ung TUPM code
  char buffer[20];
  uint32_t randomNum = attr_read(chip->tupnum_attr);
  sprintf(buffer, "TUPM-20-%d", randomNum);
  // const char *data_out = "TUPM-20-" + randomNum;
  uart_write(chip->uart0, (uint8_t *)buffer, strlen(buffer));


}

static void on_uart_rx_data(void *user_data, uint8_t byte);
static void on_uart_write_done(void *user_data);


void chip_init() {
  chip_state_t *chip = malloc(sizeof(chip_state_t));

  // pin_t inpin = pin_init("IN", INPUT_PULLDOWN);
  chip->inpin = pin_init("IN", INPUT_PULLDOWN);
  chip->tupnum_attr = attr_init("randomNum", 2669);

  const pin_watch_config_t watch_input = {
    .edge = RISING,
    .pin_change = input_changed,
    .user_data = chip,
  };
  pin_watch(chip->inpin, &watch_input);

  const uart_config_t uart_config = {
    .tx = pin_init("TX", INPUT_PULLUP),
    .rx = pin_init("RX", INPUT),
    .baud_rate = 115200,
    .rx_data = on_uart_rx_data,
    .write_done = on_uart_write_done,
    .user_data = chip,
  };
  chip->uart0 = uart_init(&uart_config);


  // TODO: Initialize the chip, set up IO pins, create timers, etc.

  printf("Hello from custom chip!\n");
}

static void on_uart_rx_data(void *user_data, uint8_t byte) {
  chip_state_t *chip = (chip_state_t*)user_data;
  printf("Incoming UART data: %d\n", byte);

}

static void on_uart_write_done(void *user_data) {
  chip_state_t *chip = (chip_state_t*)user_data;
  printf("UART done\n");
}