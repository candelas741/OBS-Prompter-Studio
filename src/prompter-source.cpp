#include "prompter-source.hpp"

#include "prompter-state.hpp"

#include <obs-module.h>
#include <plugin-support.h>

#include <QFile>
#include <QImage>
#include <QPainter>
#include <QAbstractTextDocumentLayout>
#include <QPalette>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>

#include <graphics/graphics.h>

#include <algorithm>
#include <memory>
#include <string>

namespace {
constexpr const char *SOURCE_ID = "obs_prompter_studio_source";
constexpr const char *S_TEXT = "text";
constexpr const char *S_FILE = "text_file";
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

struct PrompterSource {
	obs_source_t *source = nullptr;
	PrompterStatePtr state;
	std::string textFile;
	gs_texture_t *texture = nullptr;
	uint64_t renderedVersion = 0;
	uint32_t renderedWidth = 0;
	uint32_t renderedHeight = 0;
	bool forceRender = true;
	uint64_t tickLogCounter = 0;
	uint64_t renderLogCounter = 0;
};

QColor obsColor(uint32_t color, int alphaOverride = -1)
{
	const int r = color & 0xff;
	const int g = (color >> 8) & 0xff;
	const int b = (color >> 16) & 0xff;
	const int a = alphaOverride >= 0 ? alphaOverride : static_cast<int>((color >> 24) & 0xff);
	return QColor(r, g, b, std::clamp(a, 0, 255));
}

Qt::Alignment qtAlignment(PrompterAlign align)
{
	switch (align) {
	case PrompterAlign::Left:
		return Qt::AlignLeft;
	case PrompterAlign::Right:
		return Qt::AlignRight;
	default:
		return Qt::AlignHCenter;
	}
}

void destroyTexture(PrompterSource *ctx)
{
	if (ctx->texture) {
		gs_texture_destroy(ctx->texture);
		ctx->texture = nullptr;
	}
}

void requestSourceRefresh(PrompterSource *ctx)
{
	ctx->forceRender = true;
	obs_source_update_properties(ctx->source);
}

void buildDocument(QTextDocument &doc, const PrompterSnapshot &state, int textWidth)
{
	doc.setDocumentMargin(0);
	QFont font("Arial", state.fontSize);
	font.setStyleStrategy(QFont::PreferAntialias);
	doc.setDefaultFont(font);
	QTextOption option;
	option.setWrapMode(QTextOption::WordWrap);
	option.setAlignment(qtAlignment(state.align));
	doc.setDefaultTextOption(option);
	const std::string paragraph = state.mode == PresentationMode::Paragraph && !state.paragraphs.empty()
					      ? state.paragraphs[std::clamp(state.currentParagraphIndex, 0,
								     static_cast<int>(state.paragraphs.size()) - 1)]
					      : state.text;
	doc.setPlainText(QString::fromUtf8(paragraph.empty() ? "Prompter vacio" : paragraph.c_str()));
	doc.setTextWidth(std::max(1, textWidth));

	QTextCursor cursor(&doc);
	cursor.select(QTextCursor::Document);
	QTextBlockFormat blockFormat;
	blockFormat.setLineHeight(static_cast<int>(state.lineSpacing * 100.0),
				  QTextBlockFormat::ProportionalHeight);
	cursor.mergeBlockFormat(blockFormat);
}

void renderTexture(PrompterSource *ctx)
{
	const PrompterSnapshot state = ctx->state->snapshot();
	if (!ctx->forceRender && ctx->renderedVersion == state.version && ctx->renderedWidth == state.width &&
	    ctx->renderedHeight == state.height)
		return;

	const int width = static_cast<int>(std::max<uint32_t>(1, state.width));
	const int height = static_cast<int>(std::max<uint32_t>(1, state.height));
	const int padding = std::clamp(state.padding, 0, width / 2);
	const int textWidth = std::max(1, width - padding * 2);
	QTextDocument doc;
	buildDocument(doc, state, textWidth);
	const double contentHeight = doc.size().height() + padding * 2.0;
	ctx->state->setMaxScroll(state.mode == PresentationMode::ContinuousScroll
					 ? std::max(0.0, contentHeight - height)
					 : 0.0);

	QImage image(width, height, QImage::Format_RGBA8888);
	image.fill(Qt::transparent);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setRenderHint(QPainter::TextAntialiasing, true);
	painter.fillRect(QRect(0, 0, width, height),
			 obsColor(state.backgroundColor, state.backgroundOpacity));

	painter.save();
	if (state.mirrorHorizontal || state.mirrorVertical) {
		painter.translate(state.mirrorHorizontal ? width : 0, state.mirrorVertical ? height : 0);
		painter.scale(state.mirrorHorizontal ? -1.0 : 1.0, state.mirrorVertical ? -1.0 : 1.0);
	}

	painter.setClipRect(QRect(0, 0, width, height));
	const double y = state.mode == PresentationMode::Paragraph
				 ? std::max<double>(padding, (height - doc.size().height()) / 2.0)
				 : padding - state.scroll;
	painter.translate(padding, y);
	QAbstractTextDocumentLayout::PaintContext paintContext;
	paintContext.palette.setColor(QPalette::Text, obsColor(state.textColor));
	doc.documentLayout()->draw(&painter, paintContext);
	painter.restore();
	painter.end();

	destroyTexture(ctx);
	const uint8_t *data[] = {image.constBits()};
	ctx->texture = gs_texture_create(static_cast<uint32_t>(width), static_cast<uint32_t>(height), GS_RGBA,
					 1, data, 0);

	ctx->renderedVersion = state.version;
	ctx->renderedWidth = state.width;
	ctx->renderedHeight = state.height;
	ctx->forceRender = false;

	blog(LOG_INFO,
	     "[Prompter Studio] textura actualizada mode=%d text length=%zu scrollY=%.2f paragraph=%d/%zu font=%d color=0x%08x speed=%.2f paused=%d size=%ux%u texture=%p",
	     static_cast<int>(state.mode),
	     state.text.size(), state.scroll, state.currentParagraphIndex + 1, state.paragraphs.size(),
	     state.fontSize, state.textColor, state.speed, state.paused ? 1 : 0,
	     state.width, state.height, ctx->texture);
}

bool loadTextFile(PrompterSource *ctx, const char *path)
{
	if (!path || !*path)
		return false;
	QFile file(QString::fromUtf8(path));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		blog(LOG_WARNING, "[obs-prompter-studio] No se pudo abrir TXT: %s", path);
		return false;
	}
	ctx->state->setText(QString::fromUtf8(file.readAll()).toUtf8().constData());
	ctx->forceRender = true;
	return true;
}

bool load_file_button(obs_properties_t *, obs_property_t *, void *data)
{
	auto *ctx = static_cast<PrompterSource *>(data);
	obs_data_t *settings = obs_source_get_settings(ctx->source);
	const char *file = obs_data_get_string(settings, S_FILE);
	const bool ok = loadTextFile(ctx, file);
	if (ok) {
		obs_data_set_string(settings, S_TEXT, ctx->state->snapshot().text.c_str());
		obs_source_update(ctx->source, settings);
	}
	obs_data_release(settings);
	return true;
}

bool pause_button(obs_properties_t *, obs_property_t *, void *data)
{
	auto *ctx = static_cast<PrompterSource *>(data);
	obs_data_t *settings = obs_source_get_settings(ctx->source);
	const bool paused = obs_data_get_bool(settings, S_PAUSED);
	obs_data_set_bool(settings, S_PAUSED, !paused);
	obs_source_update(ctx->source, settings);
	obs_data_release(settings);
	return true;
}

bool reset_button(obs_properties_t *, obs_property_t *, void *data)
{
	auto *ctx = static_cast<PrompterSource *>(data);
	obs_data_t *settings = obs_source_get_settings(ctx->source);
	obs_data_set_double(settings, "scroll", 0.0);
	obs_data_set_int(settings, S_CURRENT_PARAGRAPH, 0);
	obs_source_update(ctx->source, settings);
	obs_data_release(settings);
	return true;
}

void *prompter_create(obs_data_t *settings, obs_source_t *source)
{
	auto *ctx = new PrompterSource();
	ctx->source = source;
	ctx->state = std::make_shared<PrompterState>();
	ctx->state->setSourceName(obs_source_get_name(source));
	ctx->textFile = obs_data_get_string(settings, S_FILE);
	ctx->state->applySettings(settings);
	prompter_registry_add(source, ctx->state);
	const auto snap = ctx->state->snapshot();
	blog(LOG_INFO,
	     "[Prompter Studio] Prompter Source creado name='%s' text length=%zu font=%d speed=%.2f paused=%d size=%ux%u",
	     obs_source_get_name(source), snap.text.size(), snap.fontSize, snap.speed,
	     snap.paused ? 1 : 0, snap.width, snap.height);
	return ctx;
}

void prompter_destroy(void *data)
{
	auto *ctx = static_cast<PrompterSource *>(data);
	prompter_registry_remove(ctx->source);
	obs_enter_graphics();
	destroyTexture(ctx);
	obs_leave_graphics();
	delete ctx;
}

void prompter_update(void *data, obs_data_t *settings)
{
	auto *ctx = static_cast<PrompterSource *>(data);
	ctx->textFile = obs_data_get_string(settings, S_FILE);
	ctx->state->applySettings(settings);
	ctx->forceRender = true;
	const auto snap = ctx->state->snapshot();
	blog(LOG_INFO,
	     "[Prompter Studio] settings actualizados source='%s' text length=%zu font=%d textColor=0x%08x bgColor=0x%08x bgOpacity=%d speed=%.2f paused=%d scrollY=%.2f size=%ux%u mirrorH=%d mirrorV=%d",
	     obs_source_get_name(ctx->source), snap.text.size(), snap.fontSize, snap.textColor,
	     snap.backgroundColor, snap.backgroundOpacity, snap.speed, snap.paused ? 1 : 0,
	     snap.scroll, snap.width, snap.height, snap.mirrorHorizontal ? 1 : 0,
	     snap.mirrorVertical ? 1 : 0);
}

void prompter_save(void *data, obs_data_t *settings)
{
	auto *ctx = static_cast<PrompterSource *>(data);
	ctx->state->saveSettings(settings);
	obs_data_set_string(settings, S_FILE, ctx->textFile.c_str());
}

void prompter_tick(void *data, float seconds)
{
	auto *ctx = static_cast<PrompterSource *>(data);
	PrompterSnapshot state = ctx->state->snapshot();
	if (state.mode == PresentationMode::ContinuousScroll && !state.paused && state.speed > 0.0) {
		ctx->state->setScroll(state.scroll + state.speed * seconds);
		ctx->forceRender = true;
	}
	ctx->state->setSourceName(obs_source_get_name(ctx->source));
	if (++ctx->tickLogCounter % 120 == 1) {
		const auto snap = ctx->state->snapshot();
		blog(LOG_INFO, "[Prompter Studio] video_tick source='%s' paused=%d speed=%.2f scrollY=%.2f",
		     obs_source_get_name(ctx->source), snap.paused ? 1 : 0, snap.speed, snap.scroll);
	}
}

void prompter_render(void *data, gs_effect_t *)
{
	auto *ctx = static_cast<PrompterSource *>(data);
	renderTexture(ctx);
	if (!ctx->texture)
		return;
	if (++ctx->renderLogCounter % 120 == 1) {
		const auto snap = ctx->state->snapshot();
		blog(LOG_INFO, "[Prompter Studio] render text length=%zu scrollY=%.2f size=%ux%u texture=%p",
		     snap.text.size(), snap.scroll, ctx->renderedWidth, ctx->renderedHeight,
		     ctx->texture);
	}

	gs_effect_t *default_effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_eparam_t *image = gs_effect_get_param_by_name(default_effect, "image");
	gs_effect_set_texture(image, ctx->texture);
	while (gs_effect_loop(default_effect, "Draw")) {
		gs_draw_sprite(ctx->texture, 0, ctx->renderedWidth, ctx->renderedHeight);
	}
}

uint32_t prompter_width(void *data)
{
	return static_cast<PrompterSource *>(data)->state->snapshot().width;
}

uint32_t prompter_height(void *data)
{
	return static_cast<PrompterSource *>(data)->state->snapshot().height;
}

obs_properties_t *prompter_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	obs_properties_add_int(props, S_WIDTH, "Ancho base", 160, 7680, 1);
	obs_properties_add_int(props, S_HEIGHT, "Alto base", 90, 4320, 1);
	obs_properties_add_text(props, S_TEXT, "Texto del prompter", OBS_TEXT_MULTILINE);
	obs_properties_add_path(props, S_FILE, "Cargar archivo .txt", OBS_PATH_FILE,
				"Texto (*.txt);;Todos los archivos (*.*)", nullptr);
	obs_properties_add_button(props, "load_file", "Cargar archivo", load_file_button);
	obs_properties_add_int(props, S_FONT_SIZE, "Tamano de fuente", 8, 240, 1);
	obs_properties_add_color_alpha(props, S_TEXT_COLOR, "Color de texto");
	obs_properties_add_color_alpha(props, S_BACKGROUND_COLOR, "Color de fondo");
	obs_properties_add_int_slider(props, S_BACKGROUND_OPACITY, "Opacidad de fondo", 0, 255, 1);
	obs_property_t *mode = obs_properties_add_list(props, S_PRESENTATION_MODE, "Modo de presentacion",
							 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(mode, "Scroll continuo", static_cast<long long>(PresentationMode::ContinuousScroll));
	obs_property_list_add_int(mode, "Por parrafos", static_cast<long long>(PresentationMode::Paragraph));
	obs_properties_add_int(props, S_CURRENT_PARAGRAPH, "Parrafo actual", 0, 100000, 1);
	obs_properties_add_float_slider(props, S_SPEED, "Velocidad de scroll", 0.0, 2000.0, 1.0);
	obs_properties_add_bool(props, S_PAUSED, "Pausa/Reanudar");
	obs_properties_add_button(props, "pause_button", "Pausar/Reanudar", pause_button);
	obs_properties_add_button(props, "reset_button", "Reiniciar al inicio", reset_button);
	obs_properties_add_bool(props, S_MIRROR_H, "Modo espejo horizontal");
	obs_properties_add_bool(props, S_MIRROR_V, "Modo espejo vertical");
	obs_property_t *align = obs_properties_add_list(props, S_ALIGN, "Alineacion", OBS_COMBO_TYPE_LIST,
							OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(align, "Izquierda", static_cast<long long>(PrompterAlign::Left));
	obs_property_list_add_int(align, "Centro", static_cast<long long>(PrompterAlign::Center));
	obs_property_list_add_int(align, "Derecha", static_cast<long long>(PrompterAlign::Right));
	obs_properties_add_int_slider(props, S_PADDING, "Margen interno", 0, 400, 1);
	obs_properties_add_float_slider(props, S_LINE_SPACING, "Interlineado", 0.5, 3.0, 0.05);
	return props;
}

void prompter_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, S_WIDTH, 1280);
	obs_data_set_default_int(settings, S_HEIGHT, 720);
	obs_data_set_default_string(settings, S_TEXT, "");
	obs_data_set_default_string(settings, S_FILE, "");
	obs_data_set_default_double(settings, S_SPEED, 60.0);
	obs_data_set_default_bool(settings, S_PAUSED, true);
	obs_data_set_default_int(settings, S_FONT_SIZE, 48);
	obs_data_set_default_int(settings, S_TEXT_COLOR, 0xFFFFFFFF);
	obs_data_set_default_int(settings, S_BACKGROUND_COLOR, 0x00000000);
	obs_data_set_default_int(settings, S_BACKGROUND_OPACITY, 0);
	obs_data_set_default_bool(settings, S_MIRROR_H, false);
	obs_data_set_default_bool(settings, S_MIRROR_V, false);
	obs_data_set_default_int(settings, S_ALIGN, static_cast<int>(PrompterAlign::Center));
	obs_data_set_default_int(settings, S_PADDING, 40);
	obs_data_set_default_double(settings, S_LINE_SPACING, 1.15);
	obs_data_set_default_int(settings, S_PRESENTATION_MODE,
				 static_cast<int>(PresentationMode::ContinuousScroll));
	obs_data_set_default_int(settings, S_CURRENT_PARAGRAPH, 0);
}
} // namespace

obs_source_info prompter_source_info = {};

struct PrompterSourceInfoInit {
	PrompterSourceInfoInit()
	{
		prompter_source_info.id = SOURCE_ID;
		prompter_source_info.type = OBS_SOURCE_TYPE_INPUT;
		prompter_source_info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
		prompter_source_info.get_name = [](void *) { return "Prompter Source"; };
		prompter_source_info.create = prompter_create;
		prompter_source_info.destroy = prompter_destroy;
		prompter_source_info.update = prompter_update;
		prompter_source_info.get_properties = prompter_properties;
		prompter_source_info.get_defaults = prompter_defaults;
		prompter_source_info.video_tick = prompter_tick;
		prompter_source_info.video_render = prompter_render;
		prompter_source_info.get_width = prompter_width;
		prompter_source_info.get_height = prompter_height;
		prompter_source_info.save = prompter_save;
	}
};

PrompterSourceInfoInit prompter_source_info_init;
