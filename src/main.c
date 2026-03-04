/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief GPPI (Generic Programmable Peripheral Interconnect) API demonstration for nRF54L15
 * 
 * This example demonstrates:
 * - GPPI channel allocation and connection setup
 * - Connecting GPIO input events to GPIO output tasks using GPPI (hardware connection)
 * - Button press triggers LED toggle without CPU intervention
 * - GPIOTE with GPPI for event-task connections
 * 
 * The GPPI system allows peripherals to communicate directly without CPU involvement,
 * reducing latency and power consumption.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/irq.h>
#include <nrfx_gpiote.h>
#include <helpers/nrfx_gppi.h>

LOG_MODULE_REGISTER(gppi_example, LOG_LEVEL_INF);

/* Get GPIO pin numbers from devicetree */
#define INPUT_PIN	NRF_DT_GPIOS_TO_PSEL(DT_ALIAS(sw0), gpios)
#define OUTPUT_PIN	NRF_DT_GPIOS_TO_PSEL(DT_ALIAS(led1), gpios)

/* nRF54L15 uses GPIOTE20 for cpuapp */
#define GPIOTE_INST	20
#define GPIOTE_NODE	DT_NODELABEL(gpiote20)

/* Global variables */
static nrfx_gpiote_t gpiote = NRFX_GPIOTE_INSTANCE(NRF_GPIOTE_INST_GET(GPIOTE_INST));
static nrfx_gppi_handle_t ppi_handle;
static volatile uint32_t button_callback_count = 0;

/**
 * @brief Button interrupt callback
 * 
 * This callback is triggered when the button is pressed. Note that the LED
 * toggle happens via GPPI (hardware connection) before this callback is executed.
 * This callback is just for monitoring/logging purposes.
 */
static void button_handler(nrfx_gpiote_pin_t pin,
			   nrfx_gpiote_trigger_t trigger,
			   void *context)
{
	/* Verify this is button0 */
	if (pin != INPUT_PIN) {
		return;
	}
	
	button_callback_count++;
	LOG_INF("Button event callback #%d (LED toggled via GPPI hardware connection)", 
	        button_callback_count);
}

/**
 * @brief Initialize GPIOTE peripheral
 * 
 * Configures GPIOTE channels for input (button) and output (LED)
 */
static int init_gpiote(uint8_t *p_in_channel, uint8_t *p_out_channel)
{
	int rv;
	
	LOG_INF("Initializing GPIOTE instance %d...", GPIOTE_INST);
	
	/* Connect GPIOTE IRQ to handler */
	IRQ_CONNECT(DT_IRQN(GPIOTE_NODE), DT_IRQ(GPIOTE_NODE, priority), 
	            nrfx_gpiote_irq_handler, &gpiote, 0);
	
	/* Initialize GPIOTE */
	rv = nrfx_gpiote_init(&gpiote, 0);
	if (rv != 0) {
		LOG_ERR("nrfx_gpiote_init failed: %d", rv);
		return rv;
	}
	
	/* Allocate GPIOTE channels */
	rv = nrfx_gpiote_channel_alloc(&gpiote, p_in_channel);
	if (rv != 0) {
		LOG_ERR("Failed to allocate input channel: %d", rv);
		return rv;
	}
	LOG_INF("Allocated input channel: %d", *p_in_channel);
	
	rv = nrfx_gpiote_channel_alloc(&gpiote, p_out_channel);
	if (rv != 0) {
		LOG_ERR("Failed to allocate output channel: %d", rv);
		return rv;
	}
	LOG_INF("Allocated output channel: %d", *p_out_channel);
	
	return 0;
}

/**
 * @brief Configure input pin (button) with GPIOTE
 * 
 * Sets up the button to generate events on falling edge (button press)
 */
static int configure_input_pin(uint8_t in_channel)
{
	int rv;
	
	LOG_INF("Configuring input pin %d (button)...", INPUT_PIN);
	
	/* Configure pull-up for button */
	static const nrf_gpio_pin_pull_t pull_config = NRF_GPIO_PIN_PULLUP;
	
	/* Configure trigger on falling edge (button press) */
	nrfx_gpiote_trigger_config_t trigger_config = {
		.trigger = NRFX_GPIOTE_TRIGGER_HITOLO,
		.p_in_channel = &in_channel,
	};
	
	/* Set up interrupt callback */
	static const nrfx_gpiote_handler_config_t handler_config = {
		.handler = button_handler,
	};
	
	/* Combine all configurations */
	nrfx_gpiote_input_pin_config_t input_config = {
		.p_pull_config = &pull_config,
		.p_trigger_config = &trigger_config,
		.p_handler_config = &handler_config
	};
	
	/* Apply configuration */
	rv = nrfx_gpiote_input_configure(&gpiote, INPUT_PIN, &input_config);
	if (rv != 0) {
		LOG_ERR("nrfx_gpiote_input_configure failed: %d", rv);
		return rv;
	}
	
	/* Enable trigger */
	nrfx_gpiote_trigger_enable(&gpiote, INPUT_PIN, true);
	
	LOG_INF("Input pin configured successfully");
	return 0;
}

/**
 * @brief Configure output pin (LED) with GPIOTE
 * 
 * Sets up the LED to be controlled by GPIOTE tasks (for GPPI connection)
 */
static int configure_output_pin(uint8_t out_channel)
{
	int rv;
	
	LOG_INF("Configuring output pin %d (LED)...", OUTPUT_PIN);
	
	/* Configure output pin settings */
	static const nrfx_gpiote_output_config_t output_config = {
		.drive = NRF_GPIO_PIN_S0S1,
		.input_connect = NRF_GPIO_PIN_INPUT_DISCONNECT,
		.pull = NRF_GPIO_PIN_NOPULL,
	};
	
	/* Configure task to toggle on trigger */
	const nrfx_gpiote_task_config_t task_config = {
		.task_ch = out_channel,
		.polarity = NRF_GPIOTE_POLARITY_TOGGLE,
		.init_val = 0,  /* Start with LED off (pin LOW for active-high LED) */
	};
	
	/* Apply configuration */
	rv = nrfx_gpiote_output_configure(&gpiote, OUTPUT_PIN,
					   &output_config,
					   &task_config);
	if (rv != 0) {
		LOG_ERR("nrfx_gpiote_output_configure failed: %d", rv);
		return rv;
	}
	
	/* Enable the output task */
	nrfx_gpiote_out_task_enable(&gpiote, OUTPUT_PIN);
	
	LOG_INF("Output pin configured successfully");
	return 0;
}

/**
 * @brief Setup GPPI connection between button event and LED task
 * 
 * This creates a hardware connection so button press directly toggles LED
 * without any CPU intervention.
 */
static int setup_gppi_connection(void)
{
	int rv;
	uint32_t event_addr;
	uint32_t task_addr;
	
	LOG_INF("Setting up GPPI connection...");
	
	/* Get the event address (button input event) */
	event_addr = nrfx_gpiote_in_event_address_get(&gpiote, INPUT_PIN);
	LOG_INF("Input event address: 0x%08X (GPIO%d pin %d)", 
	        event_addr, INPUT_PIN >> 5, INPUT_PIN & 0x1F);
	
	/* Get the task address (LED output toggle task) */
	task_addr = nrfx_gpiote_out_task_address_get(&gpiote, OUTPUT_PIN);
	LOG_INF("Output task address: 0x%08X (GPIO%d pin %d)", 
	        task_addr, OUTPUT_PIN >> 5, OUTPUT_PIN & 0x1F);
	
	/* Allocate and configure GPPI connection */
	rv = nrfx_gppi_conn_alloc(event_addr, task_addr, &ppi_handle);
	if (rv < 0) {
		LOG_ERR("nrfx_gppi_conn_alloc failed: %d", rv);
		return rv;
	}
	
	LOG_INF("GPPI connection allocated (handle: 0x%08X)", ppi_handle);
	
	/* Enable the GPPI connection */
	nrfx_gppi_conn_enable(ppi_handle);
	
	LOG_INF("GPPI connection enabled!");
	LOG_INF("Hardware link established: Button Event -> LED Toggle Task");
	
	return 0;
}

/**
 * @brief Main application entry point
 */
int main(void)
{
	int rv;
	uint8_t in_channel, out_channel;
	
	LOG_INF("===========================================");
	LOG_INF("GPPI Example for nRF54L15");
	LOG_INF("Board: %s", CONFIG_BOARD);
	LOG_INF("===========================================");
	LOG_INF("");
	LOG_INF("This example demonstrates GPPI (Generic Programmable");
	LOG_INF("Peripheral Interconnect) - a hardware mechanism that");
	LOG_INF("connects peripheral events to tasks without CPU intervention.");
	LOG_INF("");
	
	/* Step 1: Initialize GPIOTE */
	rv = init_gpiote(&in_channel, &out_channel);
	if (rv != 0) {
		LOG_ERR("GPIOTE initialization failed: %d", rv);
		return rv;
	}
	
	/* Step 2: Configure input pin (button) */
	rv = configure_input_pin(in_channel);
	if (rv != 0) {
		LOG_ERR("Input pin configuration failed: %d", rv);
		return rv;
	}
	
	/* Step 3: Configure output pin (LED) */
	rv = configure_output_pin(out_channel);
	if (rv != 0) {
		LOG_ERR("Output pin configuration failed: %d", rv);
		return rv;
	}
	
	/* Step 4: Setup GPPI connection */
	rv = setup_gppi_connection();
	if (rv != 0) {
		LOG_ERR("GPPI connection setup failed: %d", rv);
		return rv;
	}
	
	LOG_INF("");
	LOG_INF("===========================================");
	LOG_INF("Setup complete!");
	LOG_INF("===========================================");
	LOG_INF("Press Button 0 to toggle LED 1");
	LOG_INF("The LED will toggle via GPPI (hardware)");
	LOG_INF("with minimal latency and no CPU overhead!");
	LOG_INF("");
	
	/* Test: Manually toggle LED a few times to verify it works */
	LOG_INF("LED test: Manually toggling LED 1 three times...");
	for (int i = 0; i < 3; i++) {
		k_sleep(K_MSEC(500));
		/* Trigger the GPIOTE task to toggle the LED */
		nrfx_gpiote_out_task_trigger(&gpiote, OUTPUT_PIN);
		LOG_INF("  LED1 toggle %d", i+1);
	}
	LOG_INF("LED test complete. LED1 should be visible if working.");
	LOG_INF("");
	
	/* Main loop - just monitoring */
	uint32_t last_count = 0;
	while (1) {
		if (button_callback_count != last_count) {
			last_count = button_callback_count;
			LOG_INF("Total button press events: %d", button_callback_count);
		}
		
		k_sleep(K_MSEC(100));
	}
	
	return 0;
}
