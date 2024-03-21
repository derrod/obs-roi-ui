#include "encoder-preview.hpp"

#ifdef BUILD_STANDALONE
#include "external/display-helpers.hpp"
#include "external/qt-wrappers.hpp"
#else
#include "display-helpers.hpp"
#include "qt-wrappers.hpp"
#endif

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <graphics/matrix4.h>
#include <util/profiler.hpp>
#include <util/platform.h>

#include <QAction>
#include <QMainWindow>
#include <QObject>
#include <QMenu>
#include <random>

#include "util/util.hpp"

using namespace std;

EncoderPreview *enc_preview;

EncoderPreview::EncoderPreview(QWidget *parent)
	: QDialog(parent),
	  ui(new Ui_EncoderPreview),
	  timer(this)
{
	ui->setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	ui->hwdecodeCb->hide();

	ui->startStopBtn->setEnabled(false);

	connect(ui->close, &QPushButton::clicked, this, &EncoderPreview::close);

	connect(ui->encoderCombo, &QComboBox::currentIndexChanged, this,
		[&](int idx) { ui->startStopBtn->setEnabled(idx != -1); });

	connect(ui->startStopBtn, &QPushButton::clicked, this,
		&EncoderPreview::StartStopPreview);

	connect(ui->addToSceneBtn, &QPushButton::clicked, this, [&]() {
		OBSSourceAutoRelease scene = obs_frontend_get_current_scene();
		obs_scene_add(obs_scene_from_source(scene), previewSource);
	});

	connect(ui->refreshBtn, &QPushButton::clicked, this,
		&EncoderPreview::RefreshEncoders);

	connect(&timer, &QTimer::timeout, this, &EncoderPreview::UpdateStats);
	timer.setInterval(2000);

	CreatePreviewOutput();
	CreatePreviewSource();
}

/*
 * Private stuff
 */

void EncoderPreview::StartStopPreview(bool)
{
	if (state == INACTIVE)
		StartPreview();
	else
		StopPreview();
}

void EncoderPreview::closeEvent(QCloseEvent *event)
{
	timer.stop();
	if (!ui->runInBackGroundCb->isChecked())
		StopPreview();

	QDialog::closeEvent(event);
}

static void SetLabelText(obs_source_t *source, const char *text)
{
	if (!source)
		return;

	OBSDataAutoRelease settings = obs_source_get_settings(source);
	obs_data_set_string(settings, "text", text);
	obs_source_update(source, settings);
}

void EncoderPreview::StartPreview()
{
	OBSEncoderAutoRelease enc = obs_get_encoder_by_name(
		QT_TO_UTF8(ui->encoderCombo->currentData().toString()));

	if (ui->encoderCombo->currentIndex() == -1 || !enc) {
		ui->startStopBtn->setChecked(false);
		return;
	}

	ui->encoderCombo->setEnabled(false);
	ui->startStopBtn->setText(obs_module_text("EncoderPreview.Stop"));
	SetLabelText(waitingText, obs_module_text("EncoderPreview.Waiting"));

	state = WAITING;
	obs_encoder_set_video(enc, obs_get_video());
	obs_output_set_video_encoder(previewOut, enc);
	obs_output_start(previewOut);
}

void EncoderPreview::StopPreview()
{
	obs_output_stop(previewOut);
	while (obs_output_active(previewOut))
		this_thread::sleep_for(10ms);

	obs_output_set_video_encoder(previewOut, nullptr);

	state = INACTIVE;
	ui->encoderCombo->setEnabled(true);
	ui->startStopBtn->setText(obs_module_text("EncoderPreview.Start"));
	SetLabelText(waitingText, obs_module_text("EncoderPreview.Inactive"));
}

void EncoderPreview::RefreshEncoders()
{
	static QString itemNameTemplate("%1 (%2)");

	ui->encoderCombo->clear();
	// Find all video encoders that could reasonably be in use

	auto cb = [](void *param, obs_encoder_t *enc) {
		auto vec = static_cast<QComboBox *>(param);

		if (obs_encoder_get_type(enc) == OBS_ENCODER_VIDEO) {
			const char *display_name = obs_encoder_get_display_name(
				obs_encoder_get_id(enc));
			const char *name = obs_encoder_get_name(enc);

			vec->addItem(
				itemNameTemplate.arg(name).arg(display_name),
				name);
		}

		return true;
	};

	obs_enum_encoders(cb, ui->encoderCombo);
}

void EncoderPreview::CreateDisplay(bool recreate)
{
	// Need to recreate the display because it cannot be reset from the destroyed state
	if (recreate && ui->preview->GetDisplay() == nullptr) {
		int idx = ui->verticalLayout->indexOf(ui->preview);
		QSizePolicy policy = ui->preview->sizePolicy();
		QSize minimum = ui->preview->minimumSize();

		delete ui->preview;
		ui->preview = new OBSQTDisplay(this);
		ui->preview->setObjectName("preview");
		ui->preview->setSizePolicy(policy);
		ui->preview->setMinimumSize(minimum);

		ui->verticalLayout->insertWidget(idx, ui->preview);
	}

	auto addDrawCallback = [this]() {
		obs_display_add_draw_callback(ui->preview->GetDisplay(),
					      EncoderPreview::DrawPreview,
					      this);
	};
	connect(ui->preview, &OBSQTDisplay::DisplayCreated, addDrawCallback);
}

void EncoderPreview::DrawPreview(void *data, uint32_t cx, uint32_t cy)
{
	EncoderPreview *editor = static_cast<EncoderPreview *>(data);

	uint32_t width, height;

	if (editor->state == PLAYING) {
		width = obs_source_get_width(editor->previewSource);
		height = obs_source_get_height(editor->previewSource);
	} else {
		width = obs_source_get_width(editor->waitingText);
		height = obs_source_get_height(editor->waitingText);
	}

	int viewport_x, viewport_y;
	float scale;

	GetScaleAndCenterPos(width, height, cx, cy, viewport_x, viewport_y,
			     scale);

	int viewport_width = int(scale * float(width));
	int viewport_height = int(scale * float(height));

	gs_viewport_push();
	gs_projection_push();

	gs_ortho(0.0f, float(width), 0.0f, float(height), -100.0f, 100.0f);
	gs_set_viewport(viewport_x, viewport_y, viewport_width,
			viewport_height);

	/* Draw map texture if we have it */
	if (editor->state == PLAYING) {
		obs_source_video_render(editor->previewSource);
	} else {
		obs_source_video_render(editor->waitingText);
	}

	gs_projection_pop();
	gs_viewport_pop();
}

void EncoderPreview::CreatePreviewOutput()
{
	obs_output_info info = {};
	info.id = "encoder_preview";
	info.flags = OBS_OUTPUT_VIDEO | OBS_OUTPUT_ENCODED;
	info.get_name = [](void *) {
		return "Encoder Preview";
	};
	info.encoded_video_codecs = "h264;hevc;av1";

	info.create = [](obs_data_t *, obs_output_t *) {
		// Apparently this is valid, lol
		auto placeholder = new std::array<char, 0>;
		return static_cast<void *>(placeholder);
	};
	info.destroy = [](void *data) {
		auto placeholder = static_cast<std::array<char, 0> *>(data);
		delete placeholder;
	};
	info.start = [](void *) -> bool {
		return enc_preview->StartOutput();
	};
	info.stop = [](void *, uint64_t) {
		enc_preview->StopOutput();
	};
	info.encoded_packet = [](void *, encoder_packet *pkt) {
		enc_preview->ReceivePacket(pkt);
	};

	obs_register_output(&info);

	previewOut = obs_output_create("encoder_preview", "encoder_preview",
				       nullptr, nullptr);
}

void EncoderPreview::CreatePreviewSource()
{
	obs_source_info info = {};
	info.id = "encoder_preview_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_ASYNC_VIDEO |
			    OBS_SOURCE_DO_NOT_DUPLICATE |
			    OBS_SOURCE_CAP_DISABLED;
	info.get_name = [](void *) {
		return "Encoder Preview Source";
	};
	// Required, even though they do nothing
	info.create = [](obs_data_t *, obs_source_t *) {
		auto placeholder = new std::array<char, 0>;
		return static_cast<void *>(placeholder);
	};
	info.destroy = [](void *data) {
		auto placeholder = static_cast<std::array<char, 0> *>(data);
		delete placeholder;
	};

	obs_register_source(&info);

	previewSource = obs_source_create_private(
		info.id, "Encoder Output Preview", nullptr);
}

void EncoderPreview::UpdateStats()
{
	lock_guard lock(packetMutex);

	uint64_t now = os_gettime_ns();

	uint64_t bitsBetween = bytes * 8;
	long double timePassed =
		(long double)(now - lastStatsTime) / 1000000000.0l;
	double kbps = (long double)bitsBetween / timePassed / 1000.0l;

	QString text = obs_module_text("EncoderPreview.Bitrate");
	text += " ";
	text += loc.toString(kbps, 'f', 0);
	text += " kbps";

	ui->bitrateLbl->setText(text);
	bytes = 0;
	lastStatsTime = now;
}

/*
 * Public methods
 */

void EncoderPreview::ShowHideDialog()
{
	if (!isVisible()) {
		setVisible(true);
		CreateDisplay(true);
		if (state == INACTIVE)
			RefreshEncoders();
		timer.start();

		if (!geometry.isEmpty())
			restoreGeometry(geometry);
	} else {
		close();
	}
}

void EncoderPreview::SaveSettings(obs_data_t *data)
{
	obs_data_set_bool(data, "run_in_background",
			  ui->runInBackGroundCb->isChecked());
	obs_data_set_string(data, "window_geometry",
			    saveGeometry().toBase64().constData());
}

void EncoderPreview::LoadSettings(obs_data_t *data)
{
	ui->runInBackGroundCb->setChecked(
		obs_data_get_bool(data, "run_in_background"));

	if (const char *geo = obs_data_get_string(data, "window_geometry"))
		geometry = QByteArray::fromBase64(geo);
}

void EncoderPreview::CreateLabelSource()
{
	if (waitingText)
		return;

	OBSDataAutoRelease settings = obs_data_create();
	OBSDataAutoRelease font = obs_data_create();

#if defined(_WIN32)
	obs_data_set_string(font, "face", "Arial");
#elif defined(__APPLE__)
	obs_data_set_string(font, "face", "Helvetica");
#else
	obs_data_set_string(font, "face", "Monospace");
#endif
	obs_data_set_int(font, "flags", 1); // Bold text
	obs_data_set_int(font, "size", 256);

	obs_data_set_obj(settings, "font", font);
	obs_data_set_bool(settings, "outline", true);

#ifdef _WIN32
	obs_data_set_int(settings, "outline_color", 0x000000);
	obs_data_set_int(settings, "outline_size", 3);
	const char *text_source_id = "text_gdiplus";
#else
	const char *text_source_id = "text_ft2_source";
#endif

	obs_data_set_string(settings, "text",
			    obs_module_text("EncoderPreview.Inactive"));

	waitingText =
		obs_source_create_private(text_source_id, nullptr, settings);
}

/*
 * Output implementation
 */

bool EncoderPreview::StartOutput()
{
	if (!obs_output_can_begin_data_capture(previewOut, 0))
		return false;
	if (!obs_output_initialize_encoders(previewOut, 0))
		return false;

	OBSEncoder enc = obs_output_get_video_encoder(previewOut);
	if (!enc)
		return false;

	if (!CreateCodecContext(&codecContext, enc))
		return false;

	decoder = std::thread(&EncoderPreview::DecodeThread, this);

	return obs_output_begin_data_capture(previewOut, 0);
}

void EncoderPreview::StopOutput()
{
	obs_output_end_data_capture(previewOut);

	while (obs_output_active(previewOut))
		;

	threadKill = true;
	packetCond.notify_one();
	decoder.join();

	avcodec_free_context(&codecContext);
	video_scaler_destroy(scaler);
	scaler = nullptr;

	threadKill = false;
}

void EncoderPreview::DecodeThread()
{
	unique_lock lock(packetMutex);

	vector<packet> pkts;
	obs_source_frame frame = {};
	AVFrame *av_frame = av_frame_alloc();

	if (!SendExtraData(codecContext,
			   obs_output_get_video_encoder(previewOut))) {
		// Can fail, but doesn't seem to actually be necessary
		blog(LOG_DEBUG, "Sending extra data failed.");
	}

	bool got_first_keyframe = false;

	while (!threadKill) {
		packetCond.wait(lock);
		pkts = std::move(packets);
		lock.unlock();

		for (const packet &ctn : pkts) {
			const encoder_packet *pkt = &ctn.m_pkt;

			// Wait for keyframe to start decoding
			got_first_keyframe = got_first_keyframe ||
					     pkt->keyframe;
			if (!got_first_keyframe)
				continue;

			// ToDo: FFmpeg error handling
			if (!SendPacket(codecContext, pkt))
				continue;

			while (ReceiveFrame(codecContext, av_frame)) {
				AVFrameToSourceFrame(&frame, av_frame,
						     {pkt->timebase_num,
						      pkt->timebase_den});
				obs_source_output_video(previewSource, &frame);

				if (state != PLAYING)
					state = PLAYING;
			}
		}

		pkts.clear();
		lock.lock();
	}

	obs_source_output_video(previewSource, nullptr);
	av_frame_free(&av_frame);
}

void EncoderPreview::ReceivePacket(encoder_packet *pkt)
{
	/* Due to a bug in libobs only encoder packets from the interleaved
	 * callback are ref-counted, and since we don't need interleaving we
	 * get the raw data from the encoder, meaning we have to make our own
	 * copy (using some RAII sugar). */
	lock_guard lock(packetMutex);
	packets.emplace_back(pkt);
	packetCond.notify_one();
	bytes += pkt->size;
}

/*
 * Frontend Event Handlers
 */

static void SaveEncoderPreview(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		OBSDataAutoRelease obj = obs_data_create();
		enc_preview->SaveSettings(obj);
		obs_data_set_obj(save_data, "encoder_preview", obj);
	} else {
		OBSDataAutoRelease obj =
			obs_data_get_obj(save_data, "encoder_preview");
		if (obj)
			enc_preview->LoadSettings(obj);
	}
}

static void OBSEvent(obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		enc_preview->CreateLabelSource();
		break;
	default:
		break;
	}
}

/*
 * C stuff
 */

extern "C" void InitEncoderPreview()
{
	auto action =
		static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(
			obs_module_text("EncoderPreview")));
	auto window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());

	/* Push translation function so that strings in .ui file are translated */
	obs_frontend_push_ui_translation(obs_module_get_string);
	enc_preview = new EncoderPreview(window);
	obs_frontend_pop_ui_translation();

	obs_frontend_add_save_callback(SaveEncoderPreview, nullptr);
	obs_frontend_add_event_callback(OBSEvent, nullptr);

	QAction::connect(action, &QAction::triggered, enc_preview,
			 &EncoderPreview::ShowHideDialog);
}
