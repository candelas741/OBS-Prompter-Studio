#include "prompter-dock.hpp"
#include "prompter-hotkeys.hpp"
#include "prompter-source.hpp"

#include <obs-module.h>
#include <plugin-support.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
	obs_register_source(&prompter_source_info);
	prompter_hotkeys_register();
	prompter_dock_create();
	obs_log(LOG_INFO, "OBS Prompter Studio cargado correctamente (version %s)",
		PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	prompter_dock_destroy();
	prompter_hotkeys_unregister();
	obs_log(LOG_INFO, "OBS Prompter Studio descargado");
}
