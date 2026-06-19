#include "prompter-hotkeys.hpp"

#include "prompter-dock.hpp"

#include <obs-interaction.h>
#include <obs-module.h>

namespace {
obs_hotkey_id start_id = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id toggle_id = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id pause_id = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id resume_id = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id reset_id = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id speed_up_id = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id speed_down_id = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id forward_id = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id backward_id = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id home_id = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id end_id = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id next_paragraph_id = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id previous_paragraph_id = OBS_INVALID_HOTKEY_ID;

void withPressed(bool pressed, const char *name, void (*action)())
{
	if (!pressed)
		return;
	blog(LOG_INFO, "[Prompter Studio] Hotkey pressed: %s", name);
	action();
}

void start_cb(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	withPressed(pressed, "start", prompter_controller_start);
}

void toggle_cb(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	withPressed(pressed, "toggle pause", prompter_controller_toggle_pause);
}

void pause_cb(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	withPressed(pressed, "pause", prompter_controller_pause);
}

void resume_cb(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	withPressed(pressed, "resume", prompter_controller_resume);
}

void reset_cb(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	withPressed(pressed, "reset", prompter_controller_reset);
}

void speed_up_cb(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	withPressed(pressed, "increase speed", prompter_controller_increase_speed);
}

void speed_down_cb(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	withPressed(pressed, "decrease speed", prompter_controller_decrease_speed);
}

void forward_cb(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	withPressed(pressed, "forward", prompter_controller_step_forward);
}

void backward_cb(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	withPressed(pressed, "backward", prompter_controller_step_backward);
}

void home_cb(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	withPressed(pressed, "home", prompter_controller_go_to_start);
}

void end_cb(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	withPressed(pressed, "end", prompter_controller_go_to_end);
}

void next_paragraph_cb(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	withPressed(pressed, "next paragraph", prompter_controller_next_paragraph);
}

void previous_paragraph_cb(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	withPressed(pressed, "previous paragraph", prompter_controller_previous_paragraph);
}

void loadDefaultIfEmpty(obs_hotkey_id id, obs_key_t key, uint32_t modifiers)
{
	if (id == OBS_INVALID_HOTKEY_ID || key == OBS_KEY_NONE)
		return;

	obs_data_array_t *saved = obs_hotkey_save(id);
	const bool empty = !saved || obs_data_array_count(saved) == 0;
	obs_data_array_release(saved);
	if (!empty)
		return;

	obs_key_combination_t combo = {};
	combo.key = key;
	combo.modifiers = modifiers;
	obs_hotkey_load_bindings(id, &combo, 1);
}

void unregisterId(obs_hotkey_id &id)
{
	if (id != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(id);
		id = OBS_INVALID_HOTKEY_ID;
	}
}
} // namespace

void prompter_hotkeys_register()
{
	blog(LOG_INFO, "[Prompter Studio] Registering hotkeys");

	start_id = obs_hotkey_register_frontend("obs_prompter_studio.start",
						"Prompter Studio: iniciar", start_cb, nullptr);
	toggle_id = obs_hotkey_register_frontend("obs_prompter_studio.toggle_pause",
						 "Prompter Studio: pausar/reanudar", toggle_cb, nullptr);
	pause_id = obs_hotkey_register_frontend("obs_prompter_studio.pause",
						"Prompter Studio: pausar", pause_cb, nullptr);
	resume_id = obs_hotkey_register_frontend("obs_prompter_studio.resume",
						 "Prompter Studio: reanudar", resume_cb, nullptr);
	reset_id = obs_hotkey_register_frontend("obs_prompter_studio.reset",
						"Prompter Studio: reiniciar", reset_cb, nullptr);
	speed_up_id = obs_hotkey_register_frontend("obs_prompter_studio.speed_up",
						   "Prompter Studio: aumentar velocidad", speed_up_cb,
						   nullptr);
	speed_down_id = obs_hotkey_register_frontend("obs_prompter_studio.speed_down",
						     "Prompter Studio: reducir velocidad",
						     speed_down_cb, nullptr);
	forward_id = obs_hotkey_register_frontend("obs_prompter_studio.forward",
						  "Prompter Studio: avanzar", forward_cb, nullptr);
	backward_id = obs_hotkey_register_frontend("obs_prompter_studio.backward",
						   "Prompter Studio: retroceder", backward_cb, nullptr);
	home_id = obs_hotkey_register_frontend("obs_prompter_studio.home",
					       "Prompter Studio: volver al inicio", home_cb, nullptr);
	end_id = obs_hotkey_register_frontend("obs_prompter_studio.end",
					     "Prompter Studio: ir al final", end_cb, nullptr);
	next_paragraph_id = obs_hotkey_register_frontend("obs_prompter_studio.next_paragraph",
							    "Prompter Studio: parrafo siguiente", next_paragraph_cb,
							    nullptr);
	previous_paragraph_id = obs_hotkey_register_frontend("obs_prompter_studio.previous_paragraph",
								"Prompter Studio: parrafo anterior",
								previous_paragraph_cb, nullptr);

	const uint32_t ctrl_alt = INTERACT_CONTROL_KEY | INTERACT_ALT_KEY;
	loadDefaultIfEmpty(start_id, OBS_KEY_P, ctrl_alt);
	loadDefaultIfEmpty(toggle_id, OBS_KEY_SPACE, ctrl_alt);
	loadDefaultIfEmpty(reset_id, OBS_KEY_R, ctrl_alt);
	loadDefaultIfEmpty(speed_up_id, OBS_KEY_UP, ctrl_alt);
	loadDefaultIfEmpty(speed_down_id, OBS_KEY_DOWN, ctrl_alt);
	loadDefaultIfEmpty(forward_id, OBS_KEY_RIGHT, ctrl_alt);
	loadDefaultIfEmpty(backward_id, OBS_KEY_LEFT, ctrl_alt);
	loadDefaultIfEmpty(home_id, OBS_KEY_HOME, ctrl_alt);
	loadDefaultIfEmpty(end_id, OBS_KEY_END, ctrl_alt);
	loadDefaultIfEmpty(next_paragraph_id, OBS_KEY_PAGEDOWN, ctrl_alt);
	loadDefaultIfEmpty(previous_paragraph_id, OBS_KEY_PAGEUP, ctrl_alt);

	blog(LOG_INFO,
	     "[Prompter Studio] Suggested hotkeys: Start Ctrl+Alt+P, Toggle Ctrl+Alt+Space, Reset Ctrl+Alt+R, Speed Ctrl+Alt+Up/Down, Step Ctrl+Alt+Left/Right, Home/End Ctrl+Alt+Home/End, Paragraph Ctrl+Alt+PageUp/PageDown");
}

void prompter_hotkeys_unregister()
{
	unregisterId(start_id);
	unregisterId(toggle_id);
	unregisterId(pause_id);
	unregisterId(resume_id);
	unregisterId(reset_id);
	unregisterId(speed_up_id);
	unregisterId(speed_down_id);
	unregisterId(forward_id);
	unregisterId(backward_id);
	unregisterId(home_id);
	unregisterId(end_id);
	unregisterId(next_paragraph_id);
	unregisterId(previous_paragraph_id);
}
