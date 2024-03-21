#pragma once

#include "ui_encoder-preview.h"

#include "encoder-preview-ff-glue.hpp"

#include <QTimer>

#include <mutex>

#include <obs.hpp>
#include <media-io/video-scaler.h>


/* non-interleaved video packets are not ref-counted,
 * so this has to do as our own RAII copy mechanism. */
struct packet {
	encoder_packet m_pkt;
	std::vector<uint8_t> data;

	packet(encoder_packet *pkt)
	{
		m_pkt = *pkt;
		data.assign(pkt->data, pkt->data + pkt->size);
		m_pkt.data = data.data();
	}
};

class EncoderPreview : public QDialog {
	Q_OBJECT

	enum Status { INACTIVE, WAITING, PLAYING };

public:
	std::unique_ptr<Ui_EncoderPreview> ui;
	EncoderPreview(QWidget *parent);
	~EncoderPreview() { StopPreview(); }

	bool StartOutput();
	void StopOutput();
	void ReceivePacket(encoder_packet *pkt);

	void SaveSettings(obs_data_t *data);
	void LoadSettings(obs_data_t *data);

	void CreateLabelSource();

public slots:
	void ShowHideDialog();

protected:
	void closeEvent(QCloseEvent *event) override;

private slots:
	void StartStopPreview(bool checked);
	void UpdateStats();

private:
	void CreatePreviewOutput();
	void CreatePreviewSource();

	void StartPreview();
	void StopPreview();

	void RefreshEncoders();
	void CreateDisplay(bool recreate = false);

	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);

	OBSOutputAutoRelease previewOut;
	OBSSourceAutoRelease waitingText;
	OBSSourceAutoRelease previewSource;

	std::atomic<Status> state = INACTIVE;

	/* Output implementation related stuff */
	void DecodeThread();

	std::thread decoder;
	std::atomic_bool threadKill = false;

	std::mutex packetMutex;
	std::condition_variable packetCond;
	std::vector<packet> packets;

	uint64_t bytes = 0;
	uint64_t lastStatsTime = 0;

	AVCodecContext *codecContext = nullptr;
	video_scaler_t *scaler = nullptr;

	QTimer timer;
	QLocale loc = QLocale::system();
	QByteArray geometry;
};
