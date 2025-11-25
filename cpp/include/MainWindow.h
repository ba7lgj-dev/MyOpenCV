#pragma once

#include "CameraWorker.h"
#include "ConfigManager.h"
#include "SerialPumpController.h"
#include "WeChatNotifier.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <memory>
#include <vector>

struct CameraWidgets {
    QLabel *videoLabel{nullptr};
    QSlider *lineSlider{nullptr};
    QSlider *thresholdSlider{nullptr};
    QSlider *brightnessSlider{nullptr};
    QDoubleSpinBox *mmPerPixelSpin{nullptr};
    QDoubleSpinBox *alarmSpin{nullptr};
    QLabel *measurementLabel{nullptr};
    QCheckBox *alarmCheck{nullptr};
    QCheckBox *inflateCheck{nullptr};
    QLineEdit *portEdit{nullptr};
    QDoubleSpinBox *inflateMsSpin{nullptr};
    QDoubleSpinBox *referenceMm{nullptr};
    QSpinBox *referencePx{nullptr};
    QPushButton *scaleButton{nullptr};
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void buildUi();
    QWidget *buildCameraCard(const CameraConfig &config, int cameraId);
    void wireCamera(int idx);
    void applyConfigToUi();
    void loadConfig();
    void saveConfig();
    void onAlarm(int idx, const MeasurementResult &result);
    void updateScaleFromInput(int idx);
    void startWorkers();
    void stopWorkers();

    ConfigManager m_configManager;
    AppConfig m_config;
    std::vector<std::unique_ptr<CameraWorker>> m_workers;
    std::vector<CameraWidgets> m_cameraWidgets;
    SerialPumpController m_pump;
    WeChatNotifier m_notifier;
    QLineEdit *m_webhookEdit{nullptr};
    QPushButton *m_saveButton{nullptr};
};

