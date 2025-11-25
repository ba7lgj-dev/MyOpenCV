#include "MainWindow.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPixmap>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    loadConfig();
    buildUi();
    startWorkers();
    applyConfigToUi();
}

MainWindow::~MainWindow() {
    stopWorkers();
}

void MainWindow::buildUi() {
    QWidget *central = new QWidget(this);
    QVBoxLayout *root = new QVBoxLayout(central);

    QHBoxLayout *topRow = new QHBoxLayout();
    for (int i = 0; i < static_cast<int>(m_config.cameras.size()); ++i) {
        topRow->addWidget(buildCameraCard(m_config.cameras[i], i));
    }
    root->addLayout(topRow, 1);

    QGroupBox *globalBox = new QGroupBox(tr("全局告警与配置"), this);
    QHBoxLayout *globalLayout = new QHBoxLayout(globalBox);
    m_webhookEdit = new QLineEdit(globalBox);
    m_saveButton = new QPushButton(tr("保存参数"), globalBox);
    globalLayout->addWidget(new QLabel(tr("企业微信机器人 Webhook"), globalBox));
    globalLayout->addWidget(m_webhookEdit, 1);
    globalLayout->addWidget(m_saveButton);
    root->addWidget(globalBox, 0);

    connect(m_saveButton, &QPushButton::clicked, this, &MainWindow::saveConfig);
    connect(&m_notifier, &WeChatNotifier::notifyFinished, this, [this](const QString &result) {
        statusBar()->showMessage(tr("告警推送完成: %1").arg(result), 5000);
    });

    setCentralWidget(central);
    setWindowTitle(tr("双摄像头检测客户端"));
}

QWidget *MainWindow::buildCameraCard(const CameraConfig &config, int cameraId) {
    QGroupBox *box = new QGroupBox(tr("摄像头 %1 (%2)").arg(cameraId).arg(config.index), this);
    QVBoxLayout *layout = new QVBoxLayout(box);

    CameraWidgets widgets;
    widgets.videoLabel = new QLabel(box);
    widgets.videoLabel->setMinimumSize(320, 240);
    widgets.videoLabel->setScaledContents(true);
    layout->addWidget(widgets.videoLabel, 1);

    widgets.measurementLabel = new QLabel(tr("长度: --"), box);
    layout->addWidget(widgets.measurementLabel);

    QGridLayout *grid = new QGridLayout();
    int row = 0;
    grid->addWidget(new QLabel(tr("检测线高度%"), box), row, 0);
    widgets.lineSlider = new QSlider(Qt::Horizontal, box);
    widgets.lineSlider->setRange(0, 100);
    grid->addWidget(widgets.lineSlider, row, 1);
    ++row;

    grid->addWidget(new QLabel(tr("二值化阈值"), box), row, 0);
    widgets.thresholdSlider = new QSlider(Qt::Horizontal, box);
    widgets.thresholdSlider->setRange(0, 255);
    grid->addWidget(widgets.thresholdSlider, row, 1);
    ++row;

    grid->addWidget(new QLabel(tr("亮度偏移"), box), row, 0);
    widgets.brightnessSlider = new QSlider(Qt::Horizontal, box);
    widgets.brightnessSlider->setRange(-100, 100);
    grid->addWidget(widgets.brightnessSlider, row, 1);
    ++row;

    grid->addWidget(new QLabel(tr("mm/px 比例"), box), row, 0);
    widgets.mmPerPixelSpin = new QDoubleSpinBox(box);
    widgets.mmPerPixelSpin->setRange(0.001, 100.0);
    widgets.mmPerPixelSpin->setDecimals(4);
    grid->addWidget(widgets.mmPerPixelSpin, row, 1);
    ++row;

    grid->addWidget(new QLabel(tr("告警阈值 (mm)"), box), row, 0);
    widgets.alarmSpin = new QDoubleSpinBox(box);
    widgets.alarmSpin->setRange(0.0, 10000.0);
    grid->addWidget(widgets.alarmSpin, row, 1);
    ++row;

    widgets.alarmCheck = new QCheckBox(tr("启用告警"), box);
    grid->addWidget(widgets.alarmCheck, row, 0);
    widgets.inflateCheck = new QCheckBox(tr("低电平触发加气"), box);
    grid->addWidget(widgets.inflateCheck, row, 1);
    ++row;

    grid->addWidget(new QLabel(tr("CP2102 端口"), box), row, 0);
    widgets.portEdit = new QLineEdit(box);
    grid->addWidget(widgets.portEdit, row, 1);
    ++row;

    grid->addWidget(new QLabel(tr("加气脉冲 ms"), box), row, 0);
    widgets.inflateMsSpin = new QDoubleSpinBox(box);
    widgets.inflateMsSpin->setRange(10.0, 5000.0);
    grid->addWidget(widgets.inflateMsSpin, row, 1);
    ++row;

    grid->addWidget(new QLabel(tr("标定真实宽度 (mm)"), box), row, 0);
    widgets.referenceMm = new QDoubleSpinBox(box);
    widgets.referenceMm->setRange(0.0, 10000.0);
    grid->addWidget(widgets.referenceMm, row, 1);
    ++row;

    grid->addWidget(new QLabel(tr("标定像素长度"), box), row, 0);
    widgets.referencePx = new QSpinBox(box);
    widgets.referencePx->setRange(1, 10000);
    grid->addWidget(widgets.referencePx, row, 1);
    ++row;

    widgets.scaleButton = new QPushButton(tr("根据标定计算比例"), box);
    grid->addWidget(widgets.scaleButton, row, 0, 1, 2);

    layout->addLayout(grid);

    m_cameraWidgets.push_back(widgets);
    return box;
}

void MainWindow::wireCamera(int idx) {
    CameraWorker *worker = m_workers[idx].get();
    const CameraConfig &cfg = m_config.cameras[idx];
    worker->setCameraIndex(cfg.index);
    worker->updateProcessorSettings(cfg.settings);

    CameraControlOptions options;
    options.alarmThresholdMm = cfg.alarmThresholdMm;
    options.alarmEnabled = cfg.alarmEnabled;
    options.autoInflate = cfg.autoInflate;
    options.inflateMs = cfg.inflateMs;
    options.cp2102Port = cfg.cp2102Port;
    worker->updateControlOptions(options);

    connect(worker, &CameraWorker::frameReady, this, [this](int index, const QImage &img) {
        m_cameraWidgets[index].videoLabel->setPixmap(QPixmap::fromImage(img));
    });

    connect(worker, &CameraWorker::measurementReady, this, [this](int index, const MeasurementResult &res) {
        QString text = res.found ? tr("长度: %1 px / %2 mm (行 %3)").arg(res.whiteLengthPx).arg(res.whiteLengthMm, 0, 'f', 2).arg(res.detectedRow)
                                 : tr("状态: %1").arg(QString::fromStdString(res.message));
        m_cameraWidgets[index].measurementLabel->setText(text);
    });

    connect(worker, &CameraWorker::alarmTriggered, this, [this](int index, const MeasurementResult &res) {
        onAlarm(index, res);
    });
}

void MainWindow::applyConfigToUi() {
    m_notifier.setWebhook(m_config.webhookUrl);
    m_webhookEdit->setText(m_config.webhookUrl);

    for (int i = 0; i < static_cast<int>(m_config.cameras.size()); ++i) {
        const CameraConfig &cfg = m_config.cameras[i];
        CameraWidgets &w = m_cameraWidgets[i];
        w.lineSlider->setValue(static_cast<int>(cfg.settings.lineRatio * 100));
        w.thresholdSlider->setValue(cfg.settings.thresholdValue);
        w.brightnessSlider->setValue(cfg.settings.brightnessOffset);
        w.mmPerPixelSpin->setValue(cfg.settings.mmPerPixel);
        w.alarmSpin->setValue(cfg.alarmThresholdMm);
        w.alarmCheck->setChecked(cfg.alarmEnabled);
        w.inflateCheck->setChecked(cfg.autoInflate);
        w.portEdit->setText(cfg.cp2102Port);
        w.inflateMsSpin->setValue(cfg.inflateMs);

        connect(w.lineSlider, &QSlider::valueChanged, this, [this, i](int value) {
            m_config.cameras[i].settings.lineRatio = value / 100.0;
            m_workers[i]->updateProcessorSettings(m_config.cameras[i].settings);
        });
        connect(w.thresholdSlider, &QSlider::valueChanged, this, [this, i](int value) {
            m_config.cameras[i].settings.thresholdValue = value;
            m_workers[i]->updateProcessorSettings(m_config.cameras[i].settings);
        });
        connect(w.brightnessSlider, &QSlider::valueChanged, this, [this, i](int value) {
            m_config.cameras[i].settings.brightnessOffset = value;
            m_workers[i]->updateProcessorSettings(m_config.cameras[i].settings);
        });
        connect(w.mmPerPixelSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, i](double value) {
            m_config.cameras[i].settings.mmPerPixel = value;
            m_workers[i]->setMmPerPixel(value);
        });
        connect(w.alarmSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, i](double value) {
            m_config.cameras[i].alarmThresholdMm = value;
            CameraControlOptions opt;
            opt.alarmEnabled = m_config.cameras[i].alarmEnabled;
            opt.alarmThresholdMm = value;
            opt.autoInflate = m_config.cameras[i].autoInflate;
            opt.inflateMs = m_config.cameras[i].inflateMs;
            opt.cp2102Port = m_config.cameras[i].cp2102Port;
            m_workers[i]->updateControlOptions(opt);
        });
        connect(w.alarmCheck, &QCheckBox::toggled, this, [this, i](bool checked) {
            m_config.cameras[i].alarmEnabled = checked;
            CameraControlOptions opt;
            opt.alarmEnabled = checked;
            opt.alarmThresholdMm = m_config.cameras[i].alarmThresholdMm;
            opt.autoInflate = m_config.cameras[i].autoInflate;
            opt.inflateMs = m_config.cameras[i].inflateMs;
            opt.cp2102Port = m_config.cameras[i].cp2102Port;
            m_workers[i]->updateControlOptions(opt);
        });
        connect(w.inflateCheck, &QCheckBox::toggled, this, [this, i](bool checked) {
            m_config.cameras[i].autoInflate = checked;
            CameraControlOptions opt;
            opt.alarmEnabled = m_config.cameras[i].alarmEnabled;
            opt.alarmThresholdMm = m_config.cameras[i].alarmThresholdMm;
            opt.autoInflate = checked;
            opt.inflateMs = m_config.cameras[i].inflateMs;
            opt.cp2102Port = m_config.cameras[i].cp2102Port;
            m_workers[i]->updateControlOptions(opt);
        });
        connect(w.portEdit, &QLineEdit::textChanged, this, [this, i](const QString &text) {
            m_config.cameras[i].cp2102Port = text;
            CameraControlOptions opt;
            opt.alarmEnabled = m_config.cameras[i].alarmEnabled;
            opt.alarmThresholdMm = m_config.cameras[i].alarmThresholdMm;
            opt.autoInflate = m_config.cameras[i].autoInflate;
            opt.inflateMs = m_config.cameras[i].inflateMs;
            opt.cp2102Port = text;
            m_workers[i]->updateControlOptions(opt);
        });
        connect(w.inflateMsSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, i](double value) {
            m_config.cameras[i].inflateMs = value;
            CameraControlOptions opt;
            opt.alarmEnabled = m_config.cameras[i].alarmEnabled;
            opt.alarmThresholdMm = m_config.cameras[i].alarmThresholdMm;
            opt.autoInflate = m_config.cameras[i].autoInflate;
            opt.inflateMs = value;
            opt.cp2102Port = m_config.cameras[i].cp2102Port;
            m_workers[i]->updateControlOptions(opt);
        });
        connect(w.scaleButton, &QPushButton::clicked, this, [this, i]() {
            updateScaleFromInput(i);
        });
    }
}

void MainWindow::loadConfig() {
    m_config = m_configManager.load();
    if (m_config.cameras.empty()) {
        CameraConfig a;
        a.index = 0;
        a.name = "Camera A";
        a.settings.lineRatio = 0.6;
        a.settings.thresholdValue = 90;
        a.settings.brightnessOffset = 0;
        a.settings.mmPerPixel = 1.0;
        a.alarmThresholdMm = 0.0;
        a.alarmEnabled = false;
        a.autoInflate = false;
        a.inflateMs = 600.0;

        CameraConfig b = a;
        b.index = 1;
        b.name = "Camera B";
        m_config.cameras = {a, b};
    }
}

void MainWindow::saveConfig() {
    m_config.webhookUrl = m_webhookEdit->text();
    m_notifier.setWebhook(m_config.webhookUrl);
    m_configManager.save(m_config);
    statusBar()->showMessage(tr("参数已保存"), 3000);
}

void MainWindow::onAlarm(int idx, const MeasurementResult &result) {
    const CameraConfig &cfg = m_config.cameras[idx];
    QString title = tr("摄像头 %1 告警").arg(cfg.name);
    QString content = tr("长度 %.2fmm 低于阈值 %.2fmm").arg(result.whiteLengthMm).arg(cfg.alarmThresholdMm);
    m_notifier.sendAlarm(title, content);

    if (cfg.autoInflate) {
        m_pump.pulseLow(cfg.cp2102Port, static_cast<int>(cfg.inflateMs));
    }
}

void MainWindow::updateScaleFromInput(int idx) {
    CameraWidgets &w = m_cameraWidgets[idx];
    double mm = w.referenceMm->value();
    int px = w.referencePx->value();
    if (mm <= 0 || px <= 0) {
        QMessageBox::warning(this, tr("标定失败"), tr("请输入有效的 mm 和像素长度"));
        return;
    }
    double ratio = mm / static_cast<double>(px);
    w.mmPerPixelSpin->setValue(ratio);
    m_config.cameras[idx].settings.mmPerPixel = ratio;
    m_workers[idx]->setMmPerPixel(ratio);
}

void MainWindow::startWorkers() {
    m_workers.resize(m_config.cameras.size());
    for (int i = 0; i < static_cast<int>(m_config.cameras.size()); ++i) {
        m_workers[i] = std::make_unique<CameraWorker>(this);
        wireCamera(i);
        m_workers[i]->start();
    }
}

void MainWindow::stopWorkers() {
    for (auto &w : m_workers) {
        if (w) {
            w->stop();
        }
    }
}

