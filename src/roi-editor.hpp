#pragma once

#include <mutex>

#include "ui_roi-editor.h"

#include <obs.hpp>

class RoiListItem;
class QCloseEvent;

// ToDo add type to data as well?
struct RoiData {
	/* Scene item type*/
	QString scene_item_name;
	int64_t scene_item_id = -1;
	/* Manual type */
	uint32_t posX, posY, width, height;
	/* Center focus type */
	int64_t inner_radius;
	int64_t inner_steps;
	bool inner_aspect;
	bool inner_circle;
	int64_t outer_radius;
	int64_t outer_steps;
	float outer_priority;
	bool outer_aspect;
	int32_t center_x = -1;
	int32_t center_y = -1;
	/* Shared attributes */
	int smoothing_type;
	int smoothing_steps;
	float smoothing_priority;
	bool enabled;
	float priority;

	static RoiData fromObsData(obs_data_t *obj);
};
Q_DECLARE_METATYPE(RoiData)

class RoiEditor : public QDialog {
	Q_OBJECT

	enum Direction { Up, Down };

public:
	enum Smoothing { None, Inside, Outside, Edge };

	std::unique_ptr<Ui_ROIEditor> ui;
	RoiEditor(QWidget *parent);
	~RoiEditor()
	{
		obs_enter_graphics();
		gs_texrender_destroy(texRender);
		gs_samplerstate_destroy(pointSampler);
		gs_vertexbuffer_destroy(rectFill);
		obs_leave_graphics();
	}

	void ConnectSceneSignals();
	void LoadRoisFromOBSData(obs_data_t *obj);
	void SaveRoisToOBSData(obs_data_t *obj) const;

public slots:
	void ShowHideDialog();
	void UpdateEncoders();
	void RefreshSceneList();

private slots:
	void on_actionAddRoi_triggered();
	void on_actionRemoveRoi_triggered();
	void on_actionRoiUp_triggered();
	void on_actionRoiDown_triggered();

	void SceneSelectionChanged();
	void ItemSelected(QListWidgetItem *item, QListWidgetItem *);
	void PropertiesChanges();
	void UpdatePreview();

	void RefreshData();

private:
	void RefreshSceneItems();

	void AddRegionItem(int type);

	void RegionItemsToData();
	void RegionItemsFromData();

	std::vector<obs_encoder_roi> RegionsFromData(const std::string &uuid);
	void MoveRoiItem(Direction direction);
	void CreateDisplay(bool recreate = false);

	void closeEvent(QCloseEvent *event) override;

	static void SceneItemChanged(void *param, calldata_t *data);
	static void ItemRemovedOrAdded(void *param, calldata_t *data);
	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);
	static void CreatePreviewTexture(RoiEditor *editor, uint32_t cx,
					 uint32_t cy);

	// All signals are added/cleared at once, so just store them in a vector somewhere
	std::vector<OBSSignal> sceneSignals;

	// key is scene UUID
	std::unordered_map<std::string, std::vector<OBSDataAutoRelease>>
		roi_data;

	// Rendering stuff
	std::mutex preview_roi_mutex;
	std::vector<obs_encoder_roi> preview_roi;
	OBSWeakSourceAutoRelease previewSource;

	bool debug_draw = false;
	bool debug_draw_single = false;
	bool rebuild_texture = false;
	uint32_t texOpacity;
	uint32_t texBlockSize;

	gs_texrender_t *texRender = nullptr;
	gs_samplerstate_t *pointSampler = nullptr;
	gs_vertbuffer_t *rectFill = nullptr;

	// Qt stuff
	RoiListItem *currentItem = nullptr;
	QByteArray geometry;
};

enum ROIDataRoles { ROIData = Qt::UserRole };

class RoiListItem : public QListWidgetItem {

public:
	enum RoiItemType {
		SceneItem = QListWidgetItem::UserType,
		Manual,
		CenterFocus,
	};

	RoiListItem(int type) : QListWidgetItem(nullptr, type) {}

	QVariant data(int role) const override;
	void setData(int role, const QVariant &value) override;

private:
	RoiData roi;
};
