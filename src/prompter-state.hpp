#pragma once

#include <obs.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

enum class PrompterAlign { Left = 0, Center = 1, Right = 2 };

struct PrompterSnapshot {
	std::string text;
	double scroll = 0.0;
	double maxScroll = 0.0;
	double speed = 60.0;
	bool paused = true;
	int fontSize = 48;
	uint32_t textColor = 0xFFFFFFFF;
	uint32_t backgroundColor = 0x00000000;
	int backgroundOpacity = 0;
	bool mirrorHorizontal = false;
	bool mirrorVertical = false;
	PrompterAlign align = PrompterAlign::Center;
	int padding = 40;
	double lineSpacing = 1.15;
	uint32_t width = 1280;
	uint32_t height = 720;
	uint64_t version = 1;
};

class PrompterState {
public:
	PrompterSnapshot snapshot() const;
	void applySettings(obs_data_t *settings);
	void saveSettings(obs_data_t *settings) const;

	void setText(const std::string &text);
	void setSpeed(double speed);
	void setFontSize(int fontSize);
	void setTextColor(uint32_t color);
	void setBackgroundColor(uint32_t color);
	void setBackgroundOpacity(int opacity);
	void setScroll(double scroll);
	void setMaxScroll(double maxScroll);
	void setPaused(bool paused);
	void togglePaused();
	void reset();
	void end();
	void adjustSpeed(double delta);
	void adjustFontSize(int delta);
	void manualStep(double delta);
	void setMirrorHorizontal(bool enabled);
	void setMirrorVertical(bool enabled);

	std::string sourceName() const;
	void setSourceName(const std::string &name);

private:
	void touch();

	mutable std::mutex mutex;
	PrompterSnapshot state;
	std::string name;
};

using PrompterStatePtr = std::shared_ptr<PrompterState>;

void prompter_registry_add(obs_source_t *source, const PrompterStatePtr &state);
void prompter_registry_remove(obs_source_t *source);
bool prompter_registry_is_prompter_source(obs_source_t *source);
std::vector<PrompterStatePtr> prompter_registry_sources();
PrompterStatePtr prompter_registry_find_by_name(const std::string &name);
void prompter_registry_set_active_name(const std::string &name);
PrompterStatePtr prompter_registry_active();
