#include "ui_units.h"

static_assert(ui::display_temperature(0, true) == 32);
static_assert(ui::display_temperature(100, true) == 212);
static_assert(ui::display_temperature(-40, true) == -40);
static_assert(ui::display_temperature(21, false) == 21);
static_assert(ui::display_wind(16.09344f, true) > 9.999f &&
	ui::display_wind(16.09344f, true) < 10.001f);
static_assert(ui::display_wind(16.09344f, false) == 16.09344f);
static_assert(ui::display_pressure(1013.25f, true) > 29.91f &&
	ui::display_pressure(1013.25f, true) < 29.93f);
static_assert(ui::display_pressure(1013.25f, false) == 1013.25f);
