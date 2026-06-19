#include "prompter-dock.hpp"

#include "prompter-state.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QAbstractTextDocumentLayout>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMetaObject>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QDir>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QShowEvent>
#include <QStringList>

#include <algorithm>
#include <cstring>
#include <functional>
#include <mutex>

namespace {
constexpr const char *CONTROL_DOCK_ID = "prompter_studio_control_dock";
constexpr const char *PRIVATE_VIEW_DOCK_ID = "prompter_studio_private_view_dock";
constexpr const char *SOURCE_ID = "obs_prompter_studio_source";
constexpr const char *PRIVATE_SOURCE = "Prompter privado (sin fuente)";

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
constexpr const char *S_PRESENTATION_MODE = "presentation_mode";
constexpr const char *S_CURRENT_PARAGRAPH = "current_paragraph";

struct DockPrompterState {
	QString text;
	QString lastTxtPath;
	QString selectedSourceName = PRIVATE_SOURCE;
	double scrollY = 0.0;
	double maxScroll = 0.0;
	double speed = 60.0;
	bool paused = true;
	int fontSize = 48;
	uint32_t textColor = 0xFFFFFFFF;
	uint32_t backgroundColor = 0x00000000;
	int backgroundOpacity = 0;
	bool mirrorH = false;
	bool mirrorV = false;
	int align = static_cast<int>(PrompterAlign::Center);
	int padding = 32;
	double lineSpacing = 1.15;
	PresentationMode mode = PresentationMode::ContinuousScroll;
	QStringList paragraphs;
	int currentParagraphIndex = 0;
};

DockPrompterState sharedState;
std::mutex sharedMutex;
QWidget *controlDockWidget = nullptr;
QWidget *privateViewDockWidget = nullptr;
class PrompterPrivateViewDock;
PrompterPrivateViewDock *privateView = nullptr;
QTimer *configSaveTimer = nullptr;
bool configLoaded = false;

QString configPath()
{
	char *path = obs_module_config_path("prompter_config.json");
	QString out = path ? QString::fromUtf8(path) : QString();
	bfree(path);
	if (!out.isEmpty())
		QDir().mkpath(QFileInfo(out).absolutePath());
	return out;
}

QJsonObject stateToJson(const DockPrompterState &state)
{
	QJsonObject root;
	root["text"] = state.text;
	root["last_txt_path"] = state.lastTxtPath;
	root["selected_source"] = state.selectedSourceName;
	root["scroll_y"] = state.scrollY;
	root["speed"] = state.speed;
	root["paused"] = state.paused;
	root["font_size"] = state.fontSize;
	root["text_color"] = static_cast<double>(state.textColor);
	root["background_color"] = static_cast<double>(state.backgroundColor);
	root["background_opacity"] = state.backgroundOpacity;
	root["mirror_horizontal"] = state.mirrorH;
	root["mirror_vertical"] = state.mirrorV;
	root["align"] = state.align;
	root["padding"] = state.padding;
	root["line_spacing"] = state.lineSpacing;
	root["presentation_mode"] = static_cast<int>(state.mode);
	root["current_paragraph"] = state.currentParagraphIndex;
	return root;
}

DockPrompterState stateFromJson(const QJsonObject &root, const DockPrompterState &fallback)
{
	DockPrompterState state = fallback;
	state.text = root.value("text").toString(state.text);
	state.lastTxtPath = root.value("last_txt_path").toString(state.lastTxtPath);
	state.selectedSourceName = root.value("selected_source").toString(state.selectedSourceName);
	if (state.selectedSourceName.isEmpty())
		state.selectedSourceName = PRIVATE_SOURCE;
	state.scrollY = root.value("scroll_y").toDouble(state.scrollY);
	state.speed = root.value("speed").toDouble(state.speed);
	state.paused = root.value("paused").toBool(state.paused);
	state.fontSize = root.value("font_size").toInt(state.fontSize);
	state.textColor = static_cast<uint32_t>(root.value("text_color").toDouble(state.textColor));
	state.backgroundColor =
		static_cast<uint32_t>(root.value("background_color").toDouble(state.backgroundColor));
	state.backgroundOpacity = root.value("background_opacity").toInt(state.backgroundOpacity);
	state.mirrorH = root.value("mirror_horizontal").toBool(state.mirrorH);
	state.mirrorV = root.value("mirror_vertical").toBool(state.mirrorV);
	state.align = root.value("align").toInt(state.align);
	state.padding = root.value("padding").toInt(state.padding);
	state.lineSpacing = root.value("line_spacing").toDouble(state.lineSpacing);
	state.mode = static_cast<PresentationMode>(root.value("presentation_mode").toInt(
		static_cast<int>(state.mode)));
	state.currentParagraphIndex = root.value("current_paragraph").toInt(state.currentParagraphIndex);
	state.scrollY = std::max(0.0, state.scrollY);
	state.speed = std::clamp(state.speed, 0.0, 2000.0);
	state.fontSize = std::clamp(state.fontSize, 8, 240);
	state.backgroundOpacity = std::clamp(state.backgroundOpacity, 0, 255);
	state.padding = std::clamp(state.padding, 0, 400);
	state.lineSpacing = std::clamp(state.lineSpacing, 0.5, 3.0);
	if (state.mode != PresentationMode::Paragraph)
		state.mode = PresentationMode::ContinuousScroll;
	return state;
}

void rebuildParagraphs(DockPrompterState &state)
{
	state.paragraphs.clear();
	QStringList lines = state.text.split('\n');
	QString paragraph;
	for (QString line : lines) {
		if (line.endsWith('\r'))
			line.chop(1);
		if (line.trimmed().isEmpty()) {
			const QString cleaned = paragraph.trimmed();
			if (!cleaned.isEmpty())
				state.paragraphs.append(cleaned);
			paragraph.clear();
		} else {
			if (!paragraph.isEmpty())
				paragraph += '\n';
			paragraph += line;
		}
	}
	const QString cleaned = paragraph.trimmed();
	if (!cleaned.isEmpty())
		state.paragraphs.append(cleaned);
	if (state.paragraphs.isEmpty())
		state.currentParagraphIndex = 0;
	else
		state.currentParagraphIndex = std::clamp(state.currentParagraphIndex, 0,
							  static_cast<int>(state.paragraphs.size()) - 1);
	blog(LOG_INFO, "[Prompter Studio] Paragraphs rebuilt: %d", state.paragraphs.size());
}

QString currentParagraphText(const DockPrompterState &state)
{
	if (state.paragraphs.isEmpty())
		return {};
	return state.paragraphs.at(std::clamp(state.currentParagraphIndex, 0,
						 static_cast<int>(state.paragraphs.size()) - 1));
}

QColor colorFromObs(uint32_t color, int alphaOverride = -1)
{
	const int alpha = alphaOverride >= 0 ? alphaOverride : static_cast<int>((color >> 24) & 0xff);
	return QColor(color & 0xff, (color >> 8) & 0xff, (color >> 16) & 0xff,
		      std::clamp(alpha, 0, 255));
}

uint32_t obsFromColor(const QColor &color)
{
	return static_cast<uint32_t>(color.red()) | (static_cast<uint32_t>(color.green()) << 8) |
	       (static_cast<uint32_t>(color.blue()) << 16) |
	       (static_cast<uint32_t>(color.alpha()) << 24);
}

Qt::Alignment qtAlignment(int align)
{
	if (align == static_cast<int>(PrompterAlign::Left))
		return Qt::AlignLeft;
	if (align == static_cast<int>(PrompterAlign::Right))
		return Qt::AlignRight;
	return Qt::AlignHCenter;
}

DockPrompterState getSharedState()
{
	std::lock_guard<std::mutex> lock(sharedMutex);
	return sharedState;
}

void setSharedState(const DockPrompterState &state)
{
	std::lock_guard<std::mutex> lock(sharedMutex);
	sharedState = state;
}

void mutateSharedState(const std::function<void(DockPrompterState &)> &mutate)
{
	std::lock_guard<std::mutex> lock(sharedMutex);
	mutate(sharedState);
}

bool saveConfigNow()
{
	const QString path = configPath();
	if (path.isEmpty()) {
		blog(LOG_WARNING, "[Prompter Studio] No se pudo resolver ruta de configuracion");
		return false;
	}

	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
		blog(LOG_WARNING, "[Prompter Studio] No se pudo guardar configuracion: %s",
		     path.toUtf8().constData());
		return false;
	}

	const QJsonDocument doc(stateToJson(getSharedState()));
	file.write(doc.toJson(QJsonDocument::Indented));
	file.close();
	blog(LOG_INFO, "[Prompter Studio] Saved configuration: %s", path.toUtf8().constData());
	return true;
}

bool loadConfigNow()
{
	const QString path = configPath();
	if (path.isEmpty())
		return false;

	blog(LOG_INFO, "[Prompter Studio] Loading saved configuration: %s",
	     path.toUtf8().constData());
	QFile file(path);
	if (!file.exists()) {
		blog(LOG_INFO, "[Prompter Studio] No hay configuracion guardada todavia");
		return false;
	}
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		blog(LOG_WARNING, "[Prompter Studio] No se pudo abrir configuracion guardada");
		return false;
	}

	const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
	if (!doc.isObject()) {
		blog(LOG_WARNING, "[Prompter Studio] Configuracion guardada invalida");
		return false;
	}

	DockPrompterState state = stateFromJson(doc.object(), getSharedState());
	if (!state.lastTxtPath.isEmpty()) {
		QFile txt(state.lastTxtPath);
		if (txt.exists() && txt.open(QIODevice::ReadOnly | QIODevice::Text)) {
			state.text = QString::fromUtf8(txt.readAll());
		} else {
			blog(LOG_WARNING,
			     "[Prompter Studio] Last TXT file not found, using saved text: %s",
			     state.lastTxtPath.toUtf8().constData());
		}
	}
	rebuildParagraphs(state);
	setSharedState(state);
	configLoaded = true;
	return true;
}

void scheduleConfigSave()
{
	if (configSaveTimer) {
		configSaveTimer->start(700);
		return;
	}
	saveConfigNow();
}

void setColorButton(QPushButton *button, uint32_t color)
{
	const QColor qcolor = colorFromObs(color);
	button->setText(qcolor.name(QColor::HexArgb));
	button->setStyleSheet(QString("background:%1;").arg(qcolor.name(QColor::HexArgb)));
}

void applyDockAllowedAreas(QWidget *widget)
{
	if (!widget)
		return;

	QWidget *parent = widget->parentWidget();
	while (parent && !qobject_cast<QDockWidget *>(parent))
		parent = parent->parentWidget();

	auto *obsDock = qobject_cast<QDockWidget *>(parent);
	if (!obsDock) {
		blog(LOG_WARNING, "[Prompter Studio] QDockWidget padre no encontrado para allowed areas.");
		return;
	}

	blog(LOG_INFO, "[Prompter Studio] Applying dock allowed areas");
	obsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea |
				 Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
	obsDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
			     QDockWidget::DockWidgetClosable);
	obsDock->setMinimumSize(280, 300);
	obsDock->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

obs_source_t *sourceByName(const QString &name)
{
	if (name.isEmpty() || name == PRIVATE_SOURCE)
		return nullptr;
	return obs_get_source_by_name(name.toUtf8().constData());
}

void applyStateToSource(const DockPrompterState &state, bool scrollOnly = false)
{
	obs_source_t *source = sourceByName(state.selectedSourceName);
	if (!source)
		return;

	obs_data_t *settings = obs_source_get_settings(source);
	if (!scrollOnly) {
		obs_data_set_string(settings, S_TEXT, state.text.toUtf8().constData());
		obs_data_set_double(settings, S_SPEED, state.speed);
		// El Dock es el reloj maestro: la Source recibe scrollY y queda pausada internamente.
		obs_data_set_bool(settings, S_PAUSED, true);
		obs_data_set_int(settings, S_FONT_SIZE, state.fontSize);
		obs_data_set_int(settings, S_TEXT_COLOR, state.textColor);
		obs_data_set_int(settings, S_BACKGROUND_COLOR, state.backgroundColor);
		obs_data_set_int(settings, S_BACKGROUND_OPACITY, state.backgroundOpacity);
		obs_data_set_bool(settings, S_MIRROR_H, state.mirrorH);
		obs_data_set_bool(settings, S_MIRROR_V, state.mirrorV);
		obs_data_set_int(settings, S_ALIGN, state.align);
		obs_data_set_int(settings, S_PADDING, state.padding);
		obs_data_set_double(settings, S_LINE_SPACING, state.lineSpacing);
		obs_data_set_int(settings, S_PRESENTATION_MODE, static_cast<int>(state.mode));
		obs_data_set_int(settings, S_CURRENT_PARAGRAPH, state.currentParagraphIndex);
	}
	obs_data_set_double(settings, S_SCROLL, state.scrollY);
	obs_source_update(source, settings);
	blog(LOG_INFO, "[Prompter Studio] Applying dock settings to source: %s",
	     obs_source_get_name(source));
	obs_data_release(settings);
	obs_source_release(source);
}

class PrompterPreviewWidget : public QWidget {
public:
	explicit PrompterPreviewWidget(QWidget *parent = nullptr) : QWidget(parent)
	{
		setMinimumSize(220, 160);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		setAutoFillBackground(false);
	}

	void setState(const DockPrompterState &newState)
	{
		state = newState;
		update();
	}

	double computeMaxScrollForState(const DockPrompterState &sourceState) const
	{
		const int width = std::max(1, this->width());
		const int height = std::max(1, this->height());
		const int padding = std::clamp(sourceState.padding, 0, width / 2);
		const int textWidth = std::max(1, width - padding * 2);

		QTextDocument doc;
		buildDocument(doc, sourceState, textWidth);
		return std::max(0.0, doc.size().height() + padding * 2.0 - height);
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setRenderHint(QPainter::TextAntialiasing, true);
		painter.fillRect(rect(), colorFromObs(state.backgroundColor, state.backgroundOpacity));

		const int width = std::max(1, this->width());
		const int height = std::max(1, this->height());
		const int padding = std::clamp(state.padding, 0, width / 2);
		const int textWidth = std::max(1, width - padding * 2);

		QTextDocument doc;
		buildDocument(doc, state, textWidth);

		painter.save();
		if (state.mirrorH || state.mirrorV) {
			painter.translate(state.mirrorH ? width : 0, state.mirrorV ? height : 0);
			painter.scale(state.mirrorH ? -1.0 : 1.0, state.mirrorV ? -1.0 : 1.0);
		}
		painter.setClipRect(QRect(0, 0, width, height));
		const double y = state.mode == PresentationMode::Paragraph
					 ? std::max<double>(padding, (height - doc.size().height()) / 2.0)
					 : padding - state.scrollY;
		painter.translate(padding, y);
		QAbstractTextDocumentLayout::PaintContext context;
		context.palette.setColor(QPalette::Text, colorFromObs(state.textColor));
		doc.documentLayout()->draw(&painter, context);
		painter.restore();
	}

private:
	static void buildDocument(QTextDocument &doc, const DockPrompterState &state, int textWidth)
	{
		doc.setDocumentMargin(0);
		QFont font("Arial", state.fontSize);
		font.setStyleStrategy(QFont::PreferAntialias);
		doc.setDefaultFont(font);
		QTextOption option;
		option.setWrapMode(QTextOption::WordWrap);
		option.setAlignment(qtAlignment(state.align));
		doc.setDefaultTextOption(option);
		const QString text = state.mode == PresentationMode::Paragraph
					     ? currentParagraphText(state)
					     : state.text;
		doc.setPlainText(text.isEmpty() ? "Prompter vacio" : text);
		doc.setTextWidth(std::max(1, textWidth));

		QTextCursor cursor(&doc);
		cursor.select(QTextCursor::Document);
		QTextBlockFormat blockFormat;
		blockFormat.setLineHeight(static_cast<int>(state.lineSpacing * 100.0),
					  QTextBlockFormat::ProportionalHeight);
		cursor.mergeBlockFormat(blockFormat);
	}

	DockPrompterState state;
};

class PrompterPrivateViewDock : public QWidget {
public:
	explicit PrompterPrivateViewDock(QWidget *parent = nullptr) : QWidget(parent)
	{
		setMinimumSize(280, 300);
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
		auto *layout = new QVBoxLayout(this);
		layout->setContentsMargins(4, 4, 4, 4);
		preview = new PrompterPreviewWidget(this);
		layout->addWidget(preview, 1);
		refresh();
	}

	void refresh()
	{
		if (preview)
			preview->setState(getSharedState());
	}

	double maxScroll() const
	{
		return preview ? preview->computeMaxScrollForState(getSharedState()) : 0.0;
	}

protected:
	void showEvent(QShowEvent *event) override
	{
		QWidget::showEvent(event);
		blog(LOG_INFO, "[Prompter Studio] Private view dock shown");
	}

private:
	PrompterPreviewWidget *preview = nullptr;
};

class PrompterControlDock : public QWidget {
public:
	PrompterControlDock()
	{
		setMinimumSize(260, 300);
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
		auto *rootLayout = new QVBoxLayout(this);
		rootLayout->setContentsMargins(0, 0, 0, 0);

		auto *scrollArea = new QScrollArea(this);
		scrollArea->setWidgetResizable(true);
		scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		rootLayout->addWidget(scrollArea);

		auto *content = new QWidget(scrollArea);
		content->setMinimumWidth(240);
		content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
		auto *layout = new QVBoxLayout(content);
		layout->setContentsMargins(8, 8, 8, 8);
		layout->setSpacing(8);
		scrollArea->setWidget(content);

		auto *sourceGroup = new QGroupBox("Fuente", content);
		auto *sourceLayout = new QVBoxLayout(sourceGroup);
		sourceBox = new QComboBox(this);
		auto *refresh = new QPushButton("Actualizar", this);
		sourceBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		refresh->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		auto *sourceRow = new QHBoxLayout();
		sourceRow->addWidget(sourceBox, 1);
		sourceRow->addWidget(refresh);
		sourceLayout->addLayout(sourceRow);
		layout->addWidget(sourceGroup);

		auto *editorGroup = new QGroupBox("Editor", content);
		auto *editorLayout = new QVBoxLayout(editorGroup);
		editor = new QPlainTextEdit(this);
		editor->setPlaceholderText("Escribe o pega aqui el texto del teleprompter...");
		editor->setMinimumHeight(100);
		editor->setMaximumHeight(180);
		editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		editorLayout->addWidget(editor);
		layout->addWidget(editorGroup);

		auto *fileGroup = new QGroupBox("Archivo", content);
		auto *fileRow = new QGridLayout(fileGroup);
		auto *load = new QPushButton("Cargar TXT", this);
		auto *save = new QPushButton("Guardar TXT", this);
		load->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		save->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		fileRow->addWidget(load, 0, 0);
		fileRow->addWidget(save, 0, 1);
		layout->addWidget(fileGroup);

		auto *playGroup = new QGroupBox("Reproduccion", content);
		auto *playGrid = new QGridLayout(playGroup);
		auto *start = new QPushButton("Iniciar", this);
		auto *pause = new QPushButton("Pausar", this);
		auto *reset = new QPushButton("Reiniciar", this);
		auto *back = new QPushButton("Retroceder", this);
		auto *forward = new QPushButton("Avanzar", this);
		for (auto *button : {start, pause, reset, back, forward})
			button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		playGrid->addWidget(start, 0, 0);
		playGrid->addWidget(pause, 0, 1);
		playGrid->addWidget(reset, 1, 0, 1, 2);
		playGrid->addWidget(back, 2, 0);
		playGrid->addWidget(forward, 2, 1);
		layout->addWidget(playGroup);

		auto *modeGroup = new QGroupBox("Modo de presentacion", content);
		auto *modeLayout = new QVBoxLayout(modeGroup);
		presentationMode = new QComboBox(this);
		presentationMode->addItem("Scroll continuo", static_cast<int>(PresentationMode::ContinuousScroll));
		presentationMode->addItem("Por parrafos", static_cast<int>(PresentationMode::Paragraph));
		paragraphStatus = new QLabel(this);
		paragraphStatus->setWordWrap(true);
		auto *paragraphGrid = new QGridLayout();
		auto *previousParagraph = new QPushButton("Parrafo anterior", this);
		auto *nextParagraph = new QPushButton("Parrafo siguiente", this);
		previousParagraph->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		nextParagraph->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		paragraphGrid->addWidget(previousParagraph, 0, 0);
		paragraphGrid->addWidget(nextParagraph, 0, 1);
		modeLayout->addWidget(presentationMode);
		modeLayout->addWidget(paragraphStatus);
		modeLayout->addLayout(paragraphGrid);
		layout->addWidget(modeGroup);

		auto *settingsGroup = new QGroupBox("Ajustes", content);
		auto *form = new QFormLayout(settingsGroup);
		form->setRowWrapPolicy(QFormLayout::WrapAllRows);
		form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
		form->setLabelAlignment(Qt::AlignLeft);
		form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
		speed = new QDoubleSpinBox(this);
		speed->setRange(0.0, 2000.0);
		speed->setSingleStep(10.0);
		speed->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		fontSize = new QSpinBox(this);
		fontSize->setRange(8, 240);
		fontSize->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		textColor = new QPushButton(this);
		backgroundColor = new QPushButton(this);
		textColor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		backgroundColor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		backgroundOpacity = new QSpinBox(this);
		backgroundOpacity->setRange(0, 255);
		backgroundOpacity->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		mirrorH = new QCheckBox("Espejo horizontal", this);
		mirrorV = new QCheckBox("Espejo vertical", this);
		form->addRow("Velocidad", speed);
		form->addRow("Tamano letra", fontSize);
		form->addRow("Color texto", textColor);
		form->addRow("Color fondo", backgroundColor);
		form->addRow("Opacidad fondo", backgroundOpacity);
		form->addRow("", mirrorH);
		form->addRow("", mirrorV);
		layout->addWidget(settingsGroup);

		auto *statusGroup = new QGroupBox("Estado", content);
		auto *statusLayout = new QVBoxLayout(statusGroup);
		status = new QLabel(this);
		status->setWordWrap(true);
		status->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		statusLayout->addWidget(status);
		layout->addWidget(statusGroup);

		auto *configGroup = new QGroupBox("Configuracion", content);
		auto *configGrid = new QGridLayout(configGroup);
		auto *saveConfig = new QPushButton("Guardar configuracion", this);
		auto *restoreConfig = new QPushButton("Restaurar configuracion", this);
		saveConfig->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		restoreConfig->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		configGrid->addWidget(saveConfig, 0, 0);
		configGrid->addWidget(restoreConfig, 0, 1);
		layout->addWidget(configGroup);

		auto *hotkeyGroup = new QGroupBox("Atajos sugeridos", content);
		auto *hotkeyLayout = new QVBoxLayout(hotkeyGroup);
		auto *hotkeyToggle = new QToolButton(this);
		hotkeyToggle->setText("Atajos sugeridos");
		hotkeyToggle->setCheckable(true);
		hotkeyToggle->setChecked(false);
		hotkeyToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		hotkeyToggle->setArrowType(Qt::RightArrow);
		hotkeyToggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		auto *hotkeyHelp = new QLabel(
			"Iniciar: Ctrl + Alt + P\n"
			"Pausar/Reanudar: Espacio o Ctrl + Alt + Espacio\n"
			"Reiniciar: Ctrl + Alt + R\n"
			"Aumentar velocidad: Ctrl + Alt + ↑\n"
			"Reducir velocidad: Ctrl + Alt + ↓\n"
			"Avanzar: Ctrl + Alt + →\n"
			"Retroceder: Ctrl + Alt + ←\n"
			"Inicio: Ctrl + Alt + Home\n"
			"Final: Ctrl + Alt + End\n"
			"Parrafo anterior: Ctrl + Alt + PageUp\n"
			"Parrafo siguiente: Ctrl + Alt + PageDown",
			this);
		hotkeyHelp->setWordWrap(true);
		hotkeyHelp->setVisible(false);
		hotkeyLayout->addWidget(hotkeyToggle);
		hotkeyLayout->addWidget(hotkeyHelp);
		layout->addWidget(hotkeyGroup);
		layout->addStretch(1);

		connect(hotkeyToggle, &QToolButton::toggled, this, [hotkeyToggle, hotkeyHelp](bool open) {
			hotkeyHelp->setVisible(open);
			hotkeyToggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
		});

		connect(refresh, &QPushButton::clicked, this, [this]() { refreshSources(); });
		connect(sourceBox, &QComboBox::currentTextChanged, this, [this]() { loadSelectedSource(); });
		connect(editor, &QPlainTextEdit::textChanged, this, [this]() {
			if (syncing)
				return;
			mutateAndApply([this](DockPrompterState &state) { state.text = editor->toPlainText(); });
		});
		connect(load, &QPushButton::clicked, this, [this]() { loadTxt(); });
		connect(save, &QPushButton::clicked, this, [this]() { saveTxt(); });
		connect(start, &QPushButton::clicked, this, [this]() {
			mutateAndApply([](DockPrompterState &state) { state.paused = false; });
		});
		connect(pause, &QPushButton::clicked, this, [this]() {
			mutateAndApply([](DockPrompterState &state) { state.paused = true; });
		});
		connect(reset, &QPushButton::clicked, this, [this]() {
			mutateAndApply([](DockPrompterState &state) {
				if (state.mode == PresentationMode::Paragraph)
					state.currentParagraphIndex = 0;
				else
					state.scrollY = 0.0;
			});
		});
		connect(back, &QPushButton::clicked, this, [this]() {
			mutateAndApply([](DockPrompterState &state) {
				if (state.mode == PresentationMode::Paragraph)
					state.currentParagraphIndex--;
				else
					state.scrollY = std::max(0.0, state.scrollY - 100.0);
			});
		});
		connect(forward, &QPushButton::clicked, this, [this]() {
			mutateAndApply([](DockPrompterState &state) {
				if (state.mode == PresentationMode::Paragraph)
					state.currentParagraphIndex++;
				else
					state.scrollY = std::min(state.maxScroll, state.scrollY + 100.0);
			});
		});
		connect(previousParagraph, &QPushButton::clicked, this, [this]() {
			mutateAndApply([](DockPrompterState &state) { state.currentParagraphIndex--; });
		});
		connect(nextParagraph, &QPushButton::clicked, this, [this]() {
			mutateAndApply([](DockPrompterState &state) { state.currentParagraphIndex++; });
		});
		connect(presentationMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
			if (!syncing)
				mutateAndApply([this](DockPrompterState &state) {
					state.mode = static_cast<PresentationMode>(presentationMode->currentData().toInt());
					blog(LOG_INFO, "[Prompter Studio] Presentation mode changed: %s",
					     state.mode == PresentationMode::Paragraph ? "Paragraph" : "ContinuousScroll");
				});
		});
		connect(speed, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
			if (!syncing)
				mutateAndApply([value](DockPrompterState &state) { state.speed = value; });
		});
		connect(fontSize, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
			if (!syncing)
				mutateAndApply([value](DockPrompterState &state) { state.fontSize = value; });
		});
		connect(textColor, &QPushButton::clicked, this, [this]() {
			const auto state = getSharedState();
			const QColor chosen = QColorDialog::getColor(colorFromObs(state.textColor), this,
								     "Color de texto",
								     QColorDialog::ShowAlphaChannel);
			if (chosen.isValid())
				mutateAndApply([chosen](DockPrompterState &s) {
					s.textColor = obsFromColor(chosen);
				});
		});
		connect(backgroundColor, &QPushButton::clicked, this, [this]() {
			const auto state = getSharedState();
			const QColor chosen = QColorDialog::getColor(colorFromObs(state.backgroundColor),
								     this, "Color de fondo",
								     QColorDialog::ShowAlphaChannel);
			if (chosen.isValid())
				mutateAndApply([chosen](DockPrompterState &s) {
					s.backgroundColor = obsFromColor(chosen);
				});
		});
		connect(backgroundOpacity, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
			if (!syncing)
				mutateAndApply([value](DockPrompterState &state) {
					state.backgroundOpacity = value;
				});
		});
		connect(mirrorH, &QCheckBox::toggled, this, [this](bool checked) {
			if (!syncing)
				mutateAndApply([checked](DockPrompterState &state) {
					state.mirrorH = checked;
				});
		});
		connect(mirrorV, &QCheckBox::toggled, this, [this](bool checked) {
			if (!syncing)
				mutateAndApply([checked](DockPrompterState &state) {
					state.mirrorV = checked;
				});
		});
		connect(saveConfig, &QPushButton::clicked, this, [this]() {
			status->setText(saveConfigNow() ? "Configuracion guardada" :
						       "Error al guardar configuracion");
		});
		connect(restoreConfig, &QPushButton::clicked, this, [this]() {
			if (!loadConfigNow()) {
				status->setText("No se pudo restaurar configuracion");
				return;
			}
			refreshSources();
			if (privateView)
				privateView->refresh();
			applyStateToSource(getSharedState());
			syncControlsFromState();
			status->setText("Configuracion restaurada");
		});

		configSaveTimer = new QTimer(this);
		configSaveTimer->setSingleShot(true);
		connect(configSaveTimer, &QTimer::timeout, this, []() { saveConfigNow(); });

		tickClock.start();
		auto *timer = new QTimer(this);
		connect(timer, &QTimer::timeout, this, [this]() { onTick(); });
		timer->start(33);

		refreshSources();
		syncControlsFromState();
	}

protected:
	void showEvent(QShowEvent *event) override
	{
		QWidget::showEvent(event);
		blog(LOG_INFO, "[Prompter Studio] Control dock shown");
	}

private:
	void mutateAndApply(const std::function<void(DockPrompterState &)> &mutate, bool scrollOnly = false)
	{
		DockPrompterState state = getSharedState();
		mutate(state);
		rebuildParagraphs(state);
		state.maxScroll = privateView ? privateView->maxScroll() : state.maxScroll;
		if (state.mode == PresentationMode::ContinuousScroll)
			state.scrollY = std::clamp(state.scrollY, 0.0, state.maxScroll);
		setSharedState(state);
		if (privateView)
			privateView->refresh();
		applyStateToSource(state, scrollOnly);
		syncControlsFromState();
		scheduleConfigSave();
	}

	void refreshSources()
	{
		const QString current = sourceBox->currentText().isEmpty() ? getSharedState().selectedSourceName
									  : sourceBox->currentText();
		sourceBox->clear();
		sourceBox->addItem(PRIVATE_SOURCE);
		obs_enum_sources(
			[](void *param, obs_source_t *source) {
				auto *box = static_cast<QComboBox *>(param);
				const char *id = obs_source_get_id(source);
				if (id && strcmp(id, SOURCE_ID) == 0)
					box->addItem(QString::fromUtf8(obs_source_get_name(source)));
				return true;
			},
			sourceBox);
		const int index = sourceBox->findText(current);
		sourceBox->setCurrentIndex(index >= 0 ? index : 0);
		loadSelectedSource();
	}

	void loadSelectedSource()
	{
		DockPrompterState state = getSharedState();
		state.selectedSourceName = sourceBox->currentText();
		prompter_registry_set_active_name(state.selectedSourceName.toUtf8().constData());
		obs_source_t *source = sourceByName(state.selectedSourceName);
		if (source) {
			obs_data_t *settings = obs_source_get_settings(source);
			state.text = QString::fromUtf8(obs_data_get_string(settings, S_TEXT));
			state.scrollY = obs_data_get_double(settings, S_SCROLL);
			state.speed = obs_data_get_double(settings, S_SPEED);
			state.paused = obs_data_get_bool(settings, S_PAUSED);
			state.fontSize = static_cast<int>(obs_data_get_int(settings, S_FONT_SIZE));
			state.textColor = static_cast<uint32_t>(obs_data_get_int(settings, S_TEXT_COLOR));
			state.backgroundColor =
				static_cast<uint32_t>(obs_data_get_int(settings, S_BACKGROUND_COLOR));
			state.backgroundOpacity =
				static_cast<int>(obs_data_get_int(settings, S_BACKGROUND_OPACITY));
			state.mirrorH = obs_data_get_bool(settings, S_MIRROR_H);
			state.mirrorV = obs_data_get_bool(settings, S_MIRROR_V);
			state.align = static_cast<int>(obs_data_get_int(settings, S_ALIGN));
			state.padding = static_cast<int>(obs_data_get_int(settings, S_PADDING));
			state.lineSpacing = obs_data_get_double(settings, S_LINE_SPACING);
			state.mode = static_cast<PresentationMode>(obs_data_get_int(settings, S_PRESENTATION_MODE));
			state.currentParagraphIndex = static_cast<int>(obs_data_get_int(settings, S_CURRENT_PARAGRAPH));
			obs_data_release(settings);
			obs_source_release(source);
		}
		if (state.speed < 0.0)
			state.speed = 60.0;
		if (state.fontSize <= 0)
			state.fontSize = 48;
		if (state.lineSpacing <= 0.0)
			state.lineSpacing = 1.15;
		if (state.mode != PresentationMode::Paragraph)
			state.mode = PresentationMode::ContinuousScroll;
		rebuildParagraphs(state);
		state.maxScroll = privateView ? privateView->maxScroll() : state.maxScroll;
		if (state.mode == PresentationMode::ContinuousScroll)
			state.scrollY = std::clamp(state.scrollY, 0.0, state.maxScroll);
		setSharedState(state);
		if (privateView)
			privateView->refresh();
		syncControlsFromState();
		scheduleConfigSave();
	}

	void syncControlsFromState()
	{
		const DockPrompterState state = getSharedState();
		syncing = true;
		if (editor->toPlainText() != state.text)
			editor->setPlainText(state.text);
		speed->setValue(state.speed);
		fontSize->setValue(state.fontSize);
		presentationMode->setCurrentIndex(presentationMode->findData(static_cast<int>(state.mode)));
		backgroundOpacity->setValue(state.backgroundOpacity);
		mirrorH->setChecked(state.mirrorH);
		mirrorV->setChecked(state.mirrorV);
		setColorButton(textColor, state.textColor);
		setColorButton(backgroundColor, state.backgroundColor);
		paragraphStatus->setText(QString("Parrafo: %1 / %2")
						 .arg(state.paragraphs.isEmpty() ? 0 : state.currentParagraphIndex + 1)
						 .arg(state.paragraphs.size()));
		const QString position = state.mode == PresentationMode::Paragraph
						 ? QString("Parrafo: %1 / %2")
							   .arg(state.paragraphs.isEmpty() ? 0 : state.currentParagraphIndex + 1)
							   .arg(state.paragraphs.size())
						 : QString("Scroll: %1 / %2")
							   .arg(state.scrollY, 0, 'f', 0)
							   .arg(state.maxScroll, 0, 'f', 0);
		status->setText(QString("%1\nVelocidad: %2 | %3")
					.arg(position)
					.arg(state.speed, 0, 'f', 0)
					.arg(state.paused ? "Pausado" : "Reproduciendo"));
		syncing = false;
	}

	void onTick()
	{
		const qint64 elapsedMs = tickClock.restart();
		const double seconds = static_cast<double>(elapsedMs) / 1000.0;
		DockPrompterState state = getSharedState();
		state.maxScroll = privateView ? privateView->maxScroll() : state.maxScroll;
		if (state.mode == PresentationMode::ContinuousScroll && !state.paused)
			state.scrollY = std::min(state.maxScroll, state.scrollY + state.speed * seconds);
		setSharedState(state);
		if (privateView)
			privateView->refresh();
		applyStateToSource(state, true);
		syncControlsFromState();

		if (++tickLogCounter % 120 == 1)
			blog(LOG_INFO, "[Prompter Studio] Dock tick scrollY=%f", state.scrollY);
	}

	void loadTxt()
	{
		const QString path = QFileDialog::getOpenFileName(this, "Cargar texto", QString(),
								  "Texto (*.txt);;Todos los archivos (*.*)");
		if (path.isEmpty())
			return;
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			status->setText("Error al cargar TXT");
			return;
		}
		mutateAndApply([&file, &path](DockPrompterState &state) {
			state.text = QString::fromUtf8(file.readAll());
			state.lastTxtPath = path;
			state.scrollY = 0.0;
		});
	}

	void saveTxt()
	{
		const QString path = QFileDialog::getSaveFileName(this, "Guardar texto", "prompter.txt",
								  "Texto (*.txt);;Todos los archivos (*.*)");
		if (path.isEmpty())
			return;
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			status->setText("Error al guardar TXT");
			return;
		}
		file.write(getSharedState().text.toUtf8());
		mutateSharedState([&path](DockPrompterState &state) { state.lastTxtPath = path; });
		scheduleConfigSave();
		status->setText("TXT guardado");
	}

	QComboBox *sourceBox = nullptr;
	QPlainTextEdit *editor = nullptr;
	QDoubleSpinBox *speed = nullptr;
	QSpinBox *fontSize = nullptr;
	QComboBox *presentationMode = nullptr;
	QPushButton *textColor = nullptr;
	QPushButton *backgroundColor = nullptr;
	QSpinBox *backgroundOpacity = nullptr;
	QCheckBox *mirrorH = nullptr;
	QCheckBox *mirrorV = nullptr;
	QLabel *paragraphStatus = nullptr;
	QLabel *status = nullptr;
	QElapsedTimer tickClock;
	uint64_t tickLogCounter = 0;
	bool syncing = false;
};

void registerDock(const char *id, const char *title, QWidget *widget)
{
	blog(LOG_INFO, "[Prompter Studio] Registering dock with OBS frontend: %s", id);
	const bool ok = obs_frontend_add_dock_by_id(id, title, widget);
	if (ok) {
		blog(LOG_INFO, "[Prompter Studio] Dock registered successfully: %s", id);
		QTimer::singleShot(0, [widget]() { applyDockAllowedAreas(widget); });
		QTimer::singleShot(500, [widget]() { applyDockAllowedAreas(widget); });
	} else {
		blog(LOG_WARNING, "[Prompter Studio] No se pudo registrar dock: %s", id);
	}
}

void runControllerAction(const char *name, const std::function<void(DockPrompterState &)> &mutate,
			 bool scrollOnly = false)
{
	auto run = [name, mutate, scrollOnly]() {
		DockPrompterState state = getSharedState();
		mutate(state);
		rebuildParagraphs(state);
		state.maxScroll = privateView ? privateView->maxScroll() : state.maxScroll;
		if (state.mode == PresentationMode::ContinuousScroll)
			state.scrollY = std::clamp(state.scrollY, 0.0, state.maxScroll);
		setSharedState(state);
		if (privateView)
			privateView->refresh();
		applyStateToSource(state, scrollOnly);
		scheduleConfigSave();
		blog(LOG_INFO, "[Prompter Studio] Hotkey action applied to state and source: %s",
		     name);
	};

	if (controlDockWidget) {
		QMetaObject::invokeMethod(controlDockWidget, run, Qt::QueuedConnection);
	} else if (privateViewDockWidget) {
		QMetaObject::invokeMethod(privateViewDockWidget, run, Qt::QueuedConnection);
	} else {
		run();
	}
}
} // namespace

void prompter_dock_create()
{
	if (controlDockWidget || privateViewDockWidget)
		return;

	blog(LOG_INFO, "[Prompter Studio] Creating dock widgets");
	loadConfigNow();
	privateView = new PrompterPrivateViewDock();
	privateViewDockWidget = privateView;
	controlDockWidget = new PrompterControlDock();
	privateViewDockWidget->setObjectName(PRIVATE_VIEW_DOCK_ID);
	controlDockWidget->setObjectName(CONTROL_DOCK_ID);

	registerDock(CONTROL_DOCK_ID, "Prompter Studio Control", controlDockWidget);
	registerDock(PRIVATE_VIEW_DOCK_ID, "Prompter Studio Vista Privada", privateViewDockWidget);
}

void prompter_dock_destroy()
{
	if (configSaveTimer)
		configSaveTimer->stop();
	saveConfigNow();
	obs_frontend_remove_dock(CONTROL_DOCK_ID);
	obs_frontend_remove_dock(PRIVATE_VIEW_DOCK_ID);
	configSaveTimer = nullptr;
	controlDockWidget = nullptr;
	privateViewDockWidget = nullptr;
	privateView = nullptr;
}

void prompter_controller_start()
{
	runControllerAction("start", [](DockPrompterState &state) { state.paused = false; });
}

void prompter_controller_pause()
{
	runControllerAction("pause", [](DockPrompterState &state) { state.paused = true; });
}

void prompter_controller_resume()
{
	runControllerAction("resume", [](DockPrompterState &state) { state.paused = false; });
}

void prompter_controller_toggle_pause()
{
	runControllerAction("toggle pause",
			    [](DockPrompterState &state) { state.paused = !state.paused; });
}

void prompter_controller_reset()
{
	runControllerAction("reset", [](DockPrompterState &state) {
		if (state.mode == PresentationMode::Paragraph)
			state.currentParagraphIndex = 0;
		else
			state.scrollY = 0.0;
	});
}

void prompter_controller_increase_speed()
{
	runControllerAction("increase speed", [](DockPrompterState &state) {
		state.speed = std::clamp(state.speed + 10.0, 0.0, 2000.0);
	});
}

void prompter_controller_decrease_speed()
{
	runControllerAction("decrease speed", [](DockPrompterState &state) {
		state.speed = std::clamp(state.speed - 10.0, 0.0, 2000.0);
	});
}

void prompter_controller_step_forward()
{
	runControllerAction("forward", [](DockPrompterState &state) {
		if (state.mode == PresentationMode::Paragraph)
			state.currentParagraphIndex++;
		else
			state.scrollY = std::min(state.maxScroll, state.scrollY + 100.0);
	});
}

void prompter_controller_step_backward()
{
	runControllerAction("backward", [](DockPrompterState &state) {
		if (state.mode == PresentationMode::Paragraph)
			state.currentParagraphIndex--;
		else
			state.scrollY = std::max(0.0, state.scrollY - 100.0);
	});
}

void prompter_controller_go_to_start()
{
	runControllerAction("home", [](DockPrompterState &state) {
		if (state.mode == PresentationMode::Paragraph)
			state.currentParagraphIndex = 0;
		else
			state.scrollY = 0.0;
	});
}

void prompter_controller_go_to_end()
{
	runControllerAction("end", [](DockPrompterState &state) {
		if (state.mode == PresentationMode::Paragraph)
			state.currentParagraphIndex = state.paragraphs.size() - 1;
		else
			state.scrollY = state.maxScroll;
	});
}

void prompter_controller_next_paragraph()
{
	runControllerAction("next paragraph", [](DockPrompterState &state) { state.currentParagraphIndex++; });
}

void prompter_controller_previous_paragraph()
{
	runControllerAction("previous paragraph", [](DockPrompterState &state) { state.currentParagraphIndex--; });
}
