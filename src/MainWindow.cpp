#include "MainWindow.h"

#include <QCheckBox>
#include <QCollator>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSet>
#include <QShortcut>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace {

QImage matToQImage(const cv::Mat &mat) {
    if (mat.empty()) return {};
    if (mat.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step),
                      QImage::Format_RGB888).copy();
    }
    if (mat.type() == CV_8UC4) {
        return QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step),
                      QImage::Format_ARGB32).copy();
    }
    if (mat.type() == CV_8UC1) {
        return QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step),
                      QImage::Format_Grayscale8).copy();
    }
    cv::Mat conv;
    mat.convertTo(conv, CV_8U);
    return matToQImage(conv);
}

void setLabelPixmap(QLabel *label, const QPixmap &pix) {
    if (pix.isNull()) {
        label->clear();
        return;
    }
    const QSize target = label->size();
    if (target.width() <= 1 || target.height() <= 1) {
        label->setPixmap(pix);
        return;
    }
    label->setPixmap(pix.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

const QStringList &imageFilters() {
    static const QStringList f{"*.png", "*.jpg", "*.jpeg", "*.bmp",
                               "*.tif", "*.tiff", "*.webp"};
    return f;
}

QStringList listImages(const QString &dir) {
    return QDir(dir).entryList(imageFilters(), QDir::Files | QDir::Readable);
}

void naturalSort(QStringList &list) {
    QCollator c;
    c.setNumericMode(true);
    c.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(list.begin(), list.end(),
              [&c](const QString &a, const QString &b) { return c.compare(a, b) < 0; });
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(tr("Image Compare"));

    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *root = new QVBoxLayout(central);

    // ---- top: two file selectors ------------------------------------------
    auto *selectors = new QHBoxLayout();

    auto buildSelector = [&](const QString &placeholder, QLineEdit *&edit, QPushButton *&btn) {
        auto *row = new QHBoxLayout();
        edit = new QLineEdit();
        edit->setPlaceholderText(placeholder);
        btn = new QPushButton(tr("Browse..."));
        row->addWidget(edit, 1);
        row->addWidget(btn);
        return row;
    };

    selectors->addLayout(buildSelector(tr("Left image..."), m_leftEdit, m_leftBtn), 1);
    selectors->addLayout(buildSelector(tr("Right image..."), m_rightEdit, m_rightBtn), 1);
    root->addLayout(selectors);

    m_navLabel = new QLabel();
    m_navLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(m_navLabel);

    // ---- middle: two image previews ---------------------------------------
    auto *previews = new QHBoxLayout();
    auto makeView = [&]() {
        auto *l = new QLabel();
        l->setAlignment(Qt::AlignCenter);
        l->setMinimumSize(200, 150);
        l->setFrameShape(QFrame::StyledPanel);
        l->setStyleSheet("background:#222; color:#ddd;");
        return l;
    };
    m_leftView = makeView();
    m_rightView = makeView();
    previews->addWidget(m_leftView, 1);
    previews->addWidget(m_rightView, 1);
    root->addLayout(previews, 1);

    // ---- bottom: sliders (left) + diff view (right) -----------------------
    auto *diffRow = new QHBoxLayout();

    auto *controls = new QVBoxLayout();
    auto buildSlider = [&](const QString &title, QSlider *&slider, QLabel *&valueLabel) {
        auto *box = new QVBoxLayout();
        auto *header = new QHBoxLayout();
        auto *titleLabel = new QLabel(title);
        valueLabel = new QLabel("x1.0");
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        header->addWidget(titleLabel);
        header->addStretch(1);
        header->addWidget(valueLabel);
        slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, 100); // factor = 10^(value/50): 0->1, 50->10, 100->100
        slider->setValue(0);
        slider->setTickPosition(QSlider::TicksBelow);
        slider->setTickInterval(50);
        box->addLayout(header);
        box->addWidget(slider);
        return box;
    };

    controls->addLayout(buildSlider(tr("Top amplification (L/R)"),
                                    m_topSlider, m_topSliderLabel));
    controls->addLayout(buildSlider(tr("Bottom amplification (diff)"),
                                    m_bottomSlider, m_bottomSliderLabel));
    m_diffsOnly = new QCheckBox(tr("Diffs only (replace gray base)"));
    controls->addWidget(m_diffsOnly);
    controls->addStretch(1);
    diffRow->addLayout(controls, 1);

    m_diffView = makeView();
    m_diffView->setMinimumSize(300, 200);
    diffRow->addWidget(m_diffView, 1);
    root->addLayout(diffRow, 1);

    connect(m_topSlider, &QSlider::valueChanged, this, [this](int v) {
        m_topSliderLabel->setText(QStringLiteral("x%1").arg(sliderFactor(v), 0, 'f', 1));
        renderPreviews();
    });
    connect(m_bottomSlider, &QSlider::valueChanged, this, [this](int v) {
        m_bottomSliderLabel->setText(QStringLiteral("x%1").arg(sliderFactor(v), 0, 'f', 1));
        renderDiff();
    });
    connect(m_diffsOnly, &QCheckBox::toggled, this, [this] { renderDiff(); });

    connect(m_leftBtn, &QPushButton::clicked, this, &MainWindow::browseLeft);
    connect(m_rightBtn, &QPushButton::clicked, this, &MainWindow::browseRight);
    connect(m_leftEdit, &QLineEdit::editingFinished, this, [this] {
        updateThumb(m_leftView, m_leftEdit->text(), m_leftPix);
        tryCompare();
        rebuildNavigation();
    });
    connect(m_rightEdit, &QLineEdit::editingFinished, this, [this] {
        updateThumb(m_rightView, m_rightEdit->text(), m_rightPix);
        tryCompare();
        rebuildNavigation();
    });

    auto *prev = new QShortcut(QKeySequence(Qt::Key_PageUp), this);
    auto *next = new QShortcut(QKeySequence(Qt::Key_PageDown), this);
    prev->setContext(Qt::ApplicationShortcut);
    next->setContext(Qt::ApplicationShortcut);
    connect(prev, &QShortcut::activated, this, [this] { navigate(-1); });
    connect(next, &QShortcut::activated, this, [this] { navigate(+1); });

    updateNavLabel();
    resize(1100, 800);
}

void MainWindow::setImages(const QString &left, const QString &right) {
    m_leftEdit->setText(left);
    m_rightEdit->setText(right);
    updateThumb(m_leftView, left, m_leftPix);
    updateThumb(m_rightView, right, m_rightPix);
    tryCompare();
    rebuildNavigation();
}

void MainWindow::browseLeft() {
    const QString f = QFileDialog::getOpenFileName(
        this, tr("Choose left image"), m_leftEdit->text(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp)"), nullptr,
        QFileDialog::DontUseNativeDialog);
    if (!f.isEmpty()) {
        m_leftEdit->setText(f);
        updateThumb(m_leftView, f, m_leftPix);
        tryCompare();
        rebuildNavigation();
    }
}

void MainWindow::browseRight() {
    const QString f = QFileDialog::getOpenFileName(
        this, tr("Choose right image"), m_rightEdit->text(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp)"), nullptr,
        QFileDialog::DontUseNativeDialog);
    if (!f.isEmpty()) {
        m_rightEdit->setText(f);
        updateThumb(m_rightView, f, m_rightPix);
        tryCompare();
        rebuildNavigation();
    }
}

void MainWindow::updateThumb(QLabel *label, const QString &path, QPixmap &cache) {
    if (path.isEmpty()) {
        cache = {};
        label->clear();
        return;
    }
    cv::Mat img = cv::imread(path.toStdString(), cv::IMREAD_COLOR);
    if (img.empty()) {
        cache = {};
        label->setText(tr("Cannot load:\n%1").arg(path));
        return;
    }
    cache = QPixmap::fromImage(matToQImage(img));
    setLabelPixmap(label, cache);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    rescaleAll();
}

void MainWindow::rescaleAll() {
    if (!m_leftPix.isNull()) setLabelPixmap(m_leftView, m_leftPix);
    if (!m_rightPix.isNull()) setLabelPixmap(m_rightView, m_rightPix);
    if (!m_diffPix.isNull()) setLabelPixmap(m_diffView, m_diffPix);
}

void MainWindow::tryCompare() {
    const QString leftPath = m_leftEdit->text();
    const QString rightPath = m_rightEdit->text();
    if (leftPath.isEmpty() || rightPath.isEmpty()) return;

    cv::Mat left = cv::imread(leftPath.toStdString(), cv::IMREAD_COLOR);
    cv::Mat right = cv::imread(rightPath.toStdString(), cv::IMREAD_COLOR);
    if (left.empty() || right.empty()) {
        m_leftMat.release();
        m_rightMat.release();
        m_diffPix = {};
        m_diffView->clear();
        return;
    }

    if (left.size() != right.size()) {
        cv::resize(right, right, left.size(), 0, 0, cv::INTER_AREA);
        m_pairIdentical = false;
    } else {
        cv::Mat sameMask;
        cv::absdiff(left, right, sameMask);
        m_pairIdentical = (cv::countNonZero(sameMask.reshape(1)) == 0);
    }

    m_leftMat = left;
    m_rightMat = right;

    renderPreviews();
    renderDiff();
}

double MainWindow::sliderFactor(int value) {
    // 0 -> 1.0, 50 -> 10.0, 100 -> 100.0  (logarithmic)
    return std::pow(10.0, value / 50.0);
}

void MainWindow::renderPreviews() {
    if (m_leftMat.empty() || m_rightMat.empty()) return;

    const double k = sliderFactor(m_topSlider ? m_topSlider->value() : 0);

    if (k == 1.0) {
        m_leftPix = QPixmap::fromImage(matToQImage(m_leftMat));
        m_rightPix = QPixmap::fromImage(matToQImage(m_rightMat));
    } else {
        cv::Mat lf, rf;
        m_leftMat.convertTo(lf, CV_32F);
        m_rightMat.convertTo(rf, CV_32F);
        cv::Mat avg = (lf + rf) * 0.5f;
        cv::Mat lAmp = avg + (lf - avg) * static_cast<float>(k);
        cv::Mat rAmp = avg + (rf - avg) * static_cast<float>(k);
        cv::Mat l8, r8;
        lAmp.convertTo(l8, CV_8U); // saturates to 0..255
        rAmp.convertTo(r8, CV_8U);
        m_leftPix = QPixmap::fromImage(matToQImage(l8));
        m_rightPix = QPixmap::fromImage(matToQImage(r8));
    }

    setLabelPixmap(m_leftView, m_leftPix);
    setLabelPixmap(m_rightView, m_rightPix);
}

void MainWindow::renderDiff() {
    if (m_leftMat.empty() || m_rightMat.empty()) return;

    if (m_pairIdentical) {
        m_diffPix = {};
        m_diffView->setText(tr("Images are identical"));
        return;
    }

    cv::Mat leftGray, rightGray;
    cv::cvtColor(m_leftMat, leftGray, cv::COLOR_BGR2GRAY);
    cv::cvtColor(m_rightMat, rightGray, cv::COLOR_BGR2GRAY);

    cv::Mat avg;
    cv::addWeighted(leftGray, 0.5, rightGray, 0.5, 0, avg);

    cv::Mat diffBgr;
    cv::cvtColor(avg, diffBgr, cv::COLOR_GRAY2BGR);

    const double k = sliderFactor(m_bottomSlider ? m_bottomSlider->value() : 0);

    cv::Mat leftHigher, rightHigher;
    cv::subtract(leftGray, rightGray, leftHigher);   // >0 where left brighter
    cv::subtract(rightGray, leftGray, rightHigher);

    cv::Mat leftScaled, rightScaled;
    leftHigher.convertTo(leftScaled, CV_8U, k);   // saturates to 255
    rightHigher.convertTo(rightScaled, CV_8U, k);

    std::vector<cv::Mat> diffChannels;
    cv::split(diffBgr, diffChannels); // B, G, R
    cv::Mat &chB = diffChannels[0];
    cv::Mat &chG = diffChannels[1];
    cv::Mat &chR = diffChannels[2];
    cv::Mat leftMask = leftScaled > 0;
    cv::Mat rightMask = rightScaled > 0;

    if (m_diffsOnly && m_diffsOnly->isChecked()) {
        // Replace base pixel entirely with colored intensity.
        cv::Mat anyMask;
        cv::bitwise_or(leftMask, rightMask, anyMask);
        chB.setTo(0, anyMask);
        rightScaled.copyTo(chG, rightMask);
        chG.setTo(0, leftMask);
        leftScaled.copyTo(chR, leftMask);
        chR.setTo(0, rightMask);
    } else {
        // Blend over gray base: alpha = scaled/255. pixel = base*(1-a) + color*a.
        // For left brighter: target = (0,0,255), so:
        //   B' = base * (1-a),  G' = base * (1-a),  R' = base * (1-a) + 255 * a
        // Symmetric for right (green).
        cv::Mat baseF;
        avg.convertTo(baseF, CV_32F);

        auto applyOverlay = [&](const cv::Mat &scaled, const cv::Mat &mask,
                                int targetChannelIdx) {
            cv::Mat alpha;
            scaled.convertTo(alpha, CV_32F, 1.0 / 255.0);
            cv::Mat invA = 1.0f - alpha;
            cv::Mat baseDim = baseF.mul(invA);
            cv::Mat baseDim8;
            baseDim.convertTo(baseDim8, CV_8U);
            // For non-target channels: pixel = baseDim
            for (int ch = 0; ch < 3; ++ch) {
                if (ch == targetChannelIdx) continue;
                baseDim8.copyTo(diffChannels[ch], mask);
            }
            // For target channel: pixel = baseDim + 255 * alpha
            cv::Mat targetF = baseDim + alpha * 255.0f;
            cv::Mat target8;
            targetF.convertTo(target8, CV_8U);
            target8.copyTo(diffChannels[targetChannelIdx], mask);
        };

        applyOverlay(leftScaled, leftMask, 2);   // red channel
        applyOverlay(rightScaled, rightMask, 1); // green channel
    }

    cv::merge(diffChannels, diffBgr);

    m_diffPix = QPixmap::fromImage(matToQImage(diffBgr));
    setLabelPixmap(m_diffView, m_diffPix);
}

void MainWindow::rebuildNavigation() {
    m_navPairs.clear();
    m_navIndex = -1;

    const QFileInfo leftFi(m_leftEdit->text());
    const QFileInfo rightFi(m_rightEdit->text());
    if (!leftFi.exists() || !rightFi.exists()) return;

    const QString leftDir = leftFi.absolutePath();
    const QString rightDir = rightFi.absolutePath();
    if (leftDir == rightDir) return; // same directory: nothing meaningful to pair

    const QString leftName = leftFi.fileName();
    const QString rightName = rightFi.fileName();
    const QString leftStem = leftFi.completeBaseName();
    const QString rightStem = rightFi.completeBaseName();
    const QString leftExt = leftFi.suffix();
    const QString rightExt = rightFi.suffix();

    QStringList leftFiles = listImages(leftDir);
    QStringList rightFiles = listImages(rightDir);

    if (leftName.compare(rightName, Qt::CaseInsensitive) == 0) {
        // Mode A: same filename — intersect file names across dirs.
        QSet<QString> rightSet;
        for (const auto &n : rightFiles) rightSet.insert(n.toLower());
        QStringList common;
        for (const auto &n : leftFiles) {
            if (rightSet.contains(n.toLower())) common.append(n);
        }
        naturalSort(common);
        for (const auto &n : common) {
            m_navPairs.append({QDir(leftDir).absoluteFilePath(n),
                               QDir(rightDir).absoluteFilePath(n)});
        }
    } else if (leftStem.compare(rightStem, Qt::CaseInsensitive) == 0
               && !leftExt.isEmpty() && !rightExt.isEmpty()) {
        // Mode B: same stem, different extensions — sync by stem,
        // each side restricted to its own current extension.
        const QString leftPattern = "*." + leftExt;
        const QString rightPattern = "*." + rightExt;
        QStringList leftSameExt = QDir(leftDir).entryList({leftPattern},
                                                          QDir::Files | QDir::Readable);
        QStringList rightSameExt = QDir(rightDir).entryList({rightPattern},
                                                            QDir::Files | QDir::Readable);
        QSet<QString> rightStems;
        for (const auto &n : rightSameExt) {
            rightStems.insert(QFileInfo(n).completeBaseName().toLower());
        }
        QStringList commonStems;
        for (const auto &n : leftSameExt) {
            const QString stem = QFileInfo(n).completeBaseName();
            if (rightStems.contains(stem.toLower())) commonStems.append(stem);
        }
        naturalSort(commonStems);
        for (const auto &stem : commonStems) {
            m_navPairs.append({QDir(leftDir).absoluteFilePath(stem + "." + leftExt),
                               QDir(rightDir).absoluteFilePath(stem + "." + rightExt)});
        }
    }

    const QString curLeft = leftFi.absoluteFilePath();
    const QString curRight = rightFi.absoluteFilePath();
    for (int i = 0; i < m_navPairs.size(); ++i) {
        if (m_navPairs[i].first == curLeft && m_navPairs[i].second == curRight) {
            m_navIndex = i;
            break;
        }
    }
    updateNavLabel();
}

void MainWindow::updateNavLabel() {
    if (!m_navLabel) return;
    if (m_navPairs.isEmpty() || m_navIndex < 0) {
        m_navLabel->setText(tr("No paired navigation"));
    } else {
        m_navLabel->setText(tr("Pair %1 / %2  (PageUp / PageDown)")
                                .arg(m_navIndex + 1)
                                .arg(m_navPairs.size()));
    }
}

void MainWindow::navigate(int delta) {
    if (m_navPairs.isEmpty() || m_navIndex < 0) return;
    const int n = m_navPairs.size();
    const int next = m_navIndex + delta;
    if (next < 0 || next >= n) return;
    m_navIndex = next;
    const auto &p = m_navPairs[next];
    m_leftEdit->setText(p.first);
    m_rightEdit->setText(p.second);
    updateThumb(m_leftView, p.first, m_leftPix);
    updateThumb(m_rightView, p.second, m_rightPix);
    tryCompare();
    updateNavLabel();
}
