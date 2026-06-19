#include "prompter-state.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <map>

namespace {
constexpr const char *S_TEXT = "text";
constexpr const char *S_SCROLL = "scroll";
constexpr const char *S_SPEED = "speed";
constexpr const char *S_PAUSED = "paused";
constexpr const char *S_FONT_SIZE = "font_size";
constexpr const char *S_TEXT_COLOR = "text_color";
constexpr const char *S_BACKGROUND_COLOR = "background_color";
constexpr const char *S_BACKGROUND_OPACITY = "background_opacity";
constexpr const char *S_MIRROR_H = "mirror_horizontal";
constexpr const char *S_MIRROR_V = "mirror_vertical";
constexpr const char *S_ALIGN = "align";
constexpr const char *S_PADDING = "padding";
constexpr const char *S_LINE_SPACING = "line_spacing";
constexpr const char *S_WIDTH = "width";
constexpr const char *S_HEIGHT = "height";
constexpr const char *S_PRESENTATION_MODE = "presentation_mode";
constexpr const char *S_CURRENT_PARAGRAPH = "current_paragraph";

std::mutex registry_mutex;
std::map<obs_source_t *, PrompterStatePtr> registry;
std::string active_name;

std::string trim(const std::string &value)
{
	size_t first = 0;
	while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])))
		++first;
	size_t last = value.size();
	while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])))
		--last;
	return value.substr(first, last - first);
}
} // namespace

PrompterSnapshot PrompterState::snapshot() const
{
	std::lock_guard<std::mutex> lock(mutex);
	return state;
}

void PrompterState::applySettings(obs_data_t *settings)
{
	std::lock_guard<std::mutex> lock(mutex);
	const std::string text = obs_data_get_string(settings, S_TEXT);
	const bool textChanged = state.text != text;
	state.text = text;
	state.scroll = obs_data_get_double(settings, S_SCROLL);
	state.speed = obs_data_get_double(settings, S_SPEED);
	state.paused = obs_data_get_bool(settings, S_PAUSED);
	state.fontSize = static_cast<int>(obs_data_get_int(settings, S_FONT_SIZE));
	state.textColor = static_cast<uint32_t>(obs_data_get_int(settings, S_TEXT_COLOR));
	state.backgroundColor = static_cast<uint32_t>(obs_data_get_int(settings, S_BACKGROUND_COLOR));
	state.backgroundOpacity = static_cast<int>(obs_data_get_int(settings, S_BACKGROUND_OPACITY));
	state.mirrorHorizontal = obs_data_get_bool(settings, S_MIRROR_H);
	state.mirrorVertical = obs_data_get_bool(settings, S_MIRROR_V);
	state.align = static_cast<PrompterAlign>(obs_data_get_int(settings, S_ALIGN));
	state.padding = static_cast<int>(obs_data_get_int(settings, S_PADDING));
	state.lineSpacing = obs_data_get_double(settings, S_LINE_SPACING);
	state.width = static_cast<uint32_t>(obs_data_get_int(settings, S_WIDTH));
	state.height = static_cast<uint32_t>(obs_data_get_int(settings, S_HEIGHT));
	state.mode = static_cast<PresentationMode>(obs_data_get_int(settings, S_PRESENTATION_MODE));
	state.currentParagraphIndex = static_cast<int>(obs_data_get_int(settings, S_CURRENT_PARAGRAPH));

	if (state.speed < 0.0)
		state.speed = 60.0;
	if (state.fontSize <= 0)
		state.fontSize = 48;
	if (state.padding < 0)
		state.padding = 0;
	if (state.lineSpacing <= 0.0)
		state.lineSpacing = 1.15;
	if (state.width == 0)
		state.width = 1280;
	if (state.height == 0)
		state.height = 720;
	if (state.mode != PresentationMode::Paragraph)
		state.mode = PresentationMode::ContinuousScroll;
	if (textChanged)
		rebuildParagraphsLocked();
	else if (state.paragraphs.empty())
		state.currentParagraphIndex = 0;
	else
		state.currentParagraphIndex = std::clamp(state.currentParagraphIndex, 0,
						   static_cast<int>(state.paragraphs.size()) - 1);
	touch();
}

void PrompterState::saveSettings(obs_data_t *settings) const
{
	std::lock_guard<std::mutex> lock(mutex);
	obs_data_set_string(settings, S_TEXT, state.text.c_str());
	obs_data_set_double(settings, S_SCROLL, state.scroll);
	obs_data_set_double(settings, S_SPEED, state.speed);
	obs_data_set_bool(settings, S_PAUSED, state.paused);
	obs_data_set_int(settings, S_FONT_SIZE, state.fontSize);
	obs_data_set_int(settings, S_TEXT_COLOR, state.textColor);
	obs_data_set_int(settings, S_BACKGROUND_COLOR, state.backgroundColor);
	obs_data_set_int(settings, S_BACKGROUND_OPACITY, state.backgroundOpacity);
	obs_data_set_bool(settings, S_MIRROR_H, state.mirrorHorizontal);
	obs_data_set_bool(settings, S_MIRROR_V, state.mirrorVertical);
	obs_data_set_int(settings, S_ALIGN, static_cast<int>(state.align));
	obs_data_set_int(settings, S_PADDING, state.padding);
	obs_data_set_double(settings, S_LINE_SPACING, state.lineSpacing);
	obs_data_set_int(settings, S_WIDTH, state.width);
	obs_data_set_int(settings, S_HEIGHT, state.height);
	obs_data_set_int(settings, S_PRESENTATION_MODE, static_cast<int>(state.mode));
	obs_data_set_int(settings, S_CURRENT_PARAGRAPH, state.currentParagraphIndex);
}

void PrompterState::setText(const std::string &text)
{
	std::lock_guard<std::mutex> lock(mutex);
	state.text = text;
	rebuildParagraphsLocked();
	touch();
}

void PrompterState::setSpeed(double speed)
{
	std::lock_guard<std::mutex> lock(mutex);
	state.speed = std::clamp(speed, 0.0, 2000.0);
	touch();
}

void PrompterState::setFontSize(int fontSize)
{
	std::lock_guard<std::mutex> lock(mutex);
	state.fontSize = std::clamp(fontSize, 8, 240);
	touch();
}

void PrompterState::setTextColor(uint32_t color)
{
	std::lock_guard<std::mutex> lock(mutex);
	state.textColor = color;
	touch();
}

void PrompterState::setBackgroundColor(uint32_t color)
{
	std::lock_guard<std::mutex> lock(mutex);
	state.backgroundColor = color;
	touch();
}

void PrompterState::setBackgroundOpacity(int opacity)
{
	std::lock_guard<std::mutex> lock(mutex);
	state.backgroundOpacity = std::clamp(opacity, 0, 255);
	touch();
}

void PrompterState::setScroll(double scroll)
{
	std::lock_guard<std::mutex> lock(mutex);
	state.scroll = std::clamp(scroll, 0.0, state.maxScroll);
	touch();
}

void PrompterState::setMaxScroll(double maxScroll)
{
	std::lock_guard<std::mutex> lock(mutex);
	state.maxScroll = std::max(0.0, maxScroll);
	state.scroll = std::clamp(state.scroll, 0.0, state.maxScroll);
}

void PrompterState::setPaused(bool paused)
{
	std::lock_guard<std::mutex> lock(mutex);
	state.paused = paused;
	touch();
}

void PrompterState::togglePaused()
{
	std::lock_guard<std::mutex> lock(mutex);
	state.paused = !state.paused;
	touch();
}

void PrompterState::reset()
{
	std::lock_guard<std::mutex> lock(mutex);
	state.scroll = 0.0;
	touch();
}

void PrompterState::end()
{
	std::lock_guard<std::mutex> lock(mutex);
	state.scroll = state.maxScroll;
	touch();
}

void PrompterState::adjustSpeed(double delta)
{
	std::lock_guard<std::mutex> lock(mutex);
	state.speed = std::clamp(state.speed + delta, 0.0, 2000.0);
	touch();
}

void PrompterState::adjustFontSize(int delta)
{
	std::lock_guard<std::mutex> lock(mutex);
	state.fontSize = std::clamp(state.fontSize + delta, 8, 240);
	touch();
}

void PrompterState::manualStep(double delta)
{
	std::lock_guard<std::mutex> lock(mutex);
	state.scroll = std::clamp(state.scroll + delta, 0.0, state.maxScroll);
	touch();
}

void PrompterState::setMirrorHorizontal(bool enabled)
{
	std::lock_guard<std::mutex> lock(mutex);
	state.mirrorHorizontal = enabled;
	touch();
}

void PrompterState::setMirrorVertical(bool enabled)
{
	std::lock_guard<std::mutex> lock(mutex);
	state.mirrorVertical = enabled;
	touch();
}

void PrompterState::setPresentationMode(PresentationMode mode)
{
	std::lock_guard<std::mutex> lock(mutex);
	state.mode = mode;
	touch();
}

void PrompterState::setCurrentParagraphIndex(int index)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (state.paragraphs.empty())
		state.currentParagraphIndex = 0;
	else
		state.currentParagraphIndex = std::clamp(index, 0, static_cast<int>(state.paragraphs.size()) - 1);
	touch();
}

void PrompterState::rebuildParagraphs()
{
	std::lock_guard<std::mutex> lock(mutex);
	rebuildParagraphsLocked();
	touch();
}

void PrompterState::nextParagraph()
{
	setCurrentParagraphIndex(snapshot().currentParagraphIndex + 1);
}

void PrompterState::previousParagraph()
{
	setCurrentParagraphIndex(snapshot().currentParagraphIndex - 1);
}

void PrompterState::goToFirstParagraph()
{
	setCurrentParagraphIndex(0);
}

void PrompterState::goToLastParagraph()
{
	std::lock_guard<std::mutex> lock(mutex);
	state.currentParagraphIndex = state.paragraphs.empty() ? 0 : static_cast<int>(state.paragraphs.size()) - 1;
	touch();
}

std::string PrompterState::getCurrentParagraphText() const
{
	std::lock_guard<std::mutex> lock(mutex);
	if (state.paragraphs.empty())
		return {};
	return state.paragraphs[std::clamp(state.currentParagraphIndex, 0,
					 static_cast<int>(state.paragraphs.size()) - 1)];
}

void PrompterState::rebuildParagraphsLocked()
{
	state.paragraphs.clear();
	std::istringstream stream(state.text);
	std::string line;
	std::string paragraph;
	while (std::getline(stream, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (trim(line).empty()) {
			const std::string cleaned = trim(paragraph);
			if (!cleaned.empty())
				state.paragraphs.push_back(cleaned);
			paragraph.clear();
		} else {
			if (!paragraph.empty())
				paragraph += '\n';
			paragraph += line;
		}
	}
	const std::string cleaned = trim(paragraph);
	if (!cleaned.empty())
		state.paragraphs.push_back(cleaned);
	if (state.paragraphs.empty())
		state.currentParagraphIndex = 0;
	else
		state.currentParagraphIndex = std::clamp(state.currentParagraphIndex, 0,
						   static_cast<int>(state.paragraphs.size()) - 1);
	blog(LOG_INFO, "[Prompter Studio] Paragraphs rebuilt: %d", static_cast<int>(state.paragraphs.size()));
}

std::string PrompterState::sourceName() const
{
	std::lock_guard<std::mutex> lock(mutex);
	return name;
}

void PrompterState::setSourceName(const std::string &sourceName)
{
	std::lock_guard<std::mutex> lock(mutex);
	name = sourceName;
}

void PrompterState::touch()
{
	state.version++;
}

void prompter_registry_add(obs_source_t *source, const PrompterStatePtr &state)
{
	std::lock_guard<std::mutex> lock(registry_mutex);
	registry[source] = state;
}

void prompter_registry_remove(obs_source_t *source)
{
	std::lock_guard<std::mutex> lock(registry_mutex);
	registry.erase(source);
}

bool prompter_registry_is_prompter_source(obs_source_t *source)
{
	std::lock_guard<std::mutex> lock(registry_mutex);
	return registry.find(source) != registry.end();
}

std::vector<PrompterStatePtr> prompter_registry_sources()
{
	std::lock_guard<std::mutex> lock(registry_mutex);
	std::vector<PrompterStatePtr> out;
	for (const auto &item : registry) {
		if (item.second)
			out.push_back(item.second);
	}
	return out;
}

PrompterStatePtr prompter_registry_find_by_name(const std::string &name)
{
	std::lock_guard<std::mutex> lock(registry_mutex);
	for (const auto &item : registry) {
		if (item.second && item.second->sourceName() == name)
			return item.second;
	}
	return {};
}

void prompter_registry_set_active_name(const std::string &name)
{
	std::lock_guard<std::mutex> lock(registry_mutex);
	active_name = name;
}

PrompterStatePtr prompter_registry_active()
{
	std::lock_guard<std::mutex> lock(registry_mutex);
	for (const auto &item : registry) {
		if (item.second && item.second->sourceName() == active_name)
			return item.second;
	}
	if (!registry.empty())
		return registry.begin()->second;
	return {};
}
