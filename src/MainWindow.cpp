#include "MainWindow.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QWidget>

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

    // ---- bottom: diff view centered ---------------------------------------
    auto *diffRow = new QHBoxLayout();
    m_diffView = makeView();
    m_diffView->setMinimumSize(300, 200);
    diffRow->addStretch(1);
    diffRow->addWidget(m_diffView, 2);
    diffRow->addStretch(1);
    root->addLayout(diffRow, 1);

    connect(m_leftBtn, &QPushButton::clicked, this, &MainWindow::browseLeft);
    connect(m_rightBtn, &QPushButton::clicked, this, &MainWindow::browseRight);
    connect(m_leftEdit, &QLineEdit::editingFinished, this, [this] {
        updateThumb(m_leftView, m_leftEdit->text(), m_leftPix);
        tryCompare();
    });
    connect(m_rightEdit, &QLineEdit::editingFinished, this, [this] {
        updateThumb(m_rightView, m_rightEdit->text(), m_rightPix);
        tryCompare();
    });

    resize(1100, 800);
}

void MainWindow::setImages(const QString &left, const QString &right) {
    m_leftEdit->setText(left);
    m_rightEdit->setText(right);
    updateThumb(m_leftView, left, m_leftPix);
    updateThumb(m_rightView, right, m_rightPix);
    tryCompare();
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
        m_diffPix = {};
        m_diffView->clear();
        return;
    }

    if (left.size() != right.size()) {
        cv::resize(right, right, left.size(), 0, 0, cv::INTER_AREA);
    } else {
        cv::Mat sameMask;
        cv::absdiff(left, right, sameMask);
        if (cv::countNonZero(sameMask.reshape(1)) == 0) {
            m_diffPix = {};
            m_diffView->setText(tr("Images are identical"));
            return;
        }
    }

    cv::Mat leftGray, rightGray;
    cv::cvtColor(left, leftGray, cv::COLOR_BGR2GRAY);
    cv::cvtColor(right, rightGray, cv::COLOR_BGR2GRAY);

    // Build diff: shared content shown as-is in grayscale; red/green overlay only where pixels differ.
    cv::Mat avg;
    cv::addWeighted(leftGray, 0.5, rightGray, 0.5, 0, avg);

    cv::Mat diffBgr;
    cv::cvtColor(avg, diffBgr, cv::COLOR_GRAY2BGR);

    cv::Mat leftHigher, rightHigher;
    cv::subtract(leftGray, rightGray, leftHigher);   // >0 where left brighter
    cv::subtract(rightGray, leftGray, rightHigher);

    const int threshold = 8;
    cv::Mat leftMask = leftHigher > threshold;   // 8UC1, 0/255
    cv::Mat rightMask = rightHigher > threshold;

    // Apply red (BGR: 0,0,255) where left mask, green (0,255,0) where right mask.
    diffBgr.setTo(cv::Scalar(0, 0, 255), leftMask);
    diffBgr.setTo(cv::Scalar(0, 255, 0), rightMask);

    m_diffPix = QPixmap::fromImage(matToQImage(diffBgr));
    setLabelPixmap(m_diffView, m_diffPix);
}