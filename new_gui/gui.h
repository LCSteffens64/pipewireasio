#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pwasio_gui;

// The configuration shared between the GUI and its client.
struct pwasio_gui_conf {
	// Client's state pointer
	void *user;
	// Called when the GUI wants to close. Called after `apply_config`,
	// if confirmed. The application should call pwasio_destroy_gui in
	// response.
	void (*closed)(struct pwasio_gui_conf *gui);
	// Apply the config using state in this struct.
	void (*apply_config)(struct pwasio_gui_conf *gui);
	// The application should fill state in this struct with
	// stored configuration, or with defaults if not available.
	void (*load_config)(struct pwasio_gui_conf *gui);

	struct pw_main_loop *pw_main_loop;
	// The state
	uint32_t cf_buffer_size;
};

// The config must persist for as long as the GUI is live.
struct pwasio_gui *pwasio_init_gui(struct pwasio_gui_conf *conf);
void pwasio_destroy_gui(struct pwasio_gui *gui);

#ifdef __cplusplus
}
#endif
