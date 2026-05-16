#pragma once

#include <QMainWindow>
#include <QString>
#include <QVector>
#include <utility>

#include <opencv2/core.hpp>

class QLineEdit;
class QLabel;
class QPushButton;
class QSlider;
class QCheckBox;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void setImages(const QString &left, const QString &right);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void browseLeft();
    void browseRight();

private:
    void updateThumb(QLabel *label, const QString &path, QPixmap &cache);
    void rescaleAll();
    void tryCompare();
    void rebuildNavigation();
    void navigate(int delta);
    void renderPreviews();
    void renderDiff();
    static double sliderFactor(int value);

    QVector<QPair<QString, QString>> m_navPairs; // absolute (leftPath, rightPath)
    int m_navIndex{-1};

    QPixmap m_leftPix;
    QPixmap m_rightPix;
    QPixmap m_diffPix;

    QLineEdit *m_leftEdit{};
    QLineEdit *m_rightEdit{};
    QPushButton *m_leftBtn{};
    QPushButton *m_rightBtn{};
    QLabel *m_leftView{};
    QLabel *m_rightView{};
    QLabel *m_diffView{};
    QLabel *m_navLabel{};

    QSlider *m_topSlider{};
    QSlider *m_bottomSlider{};
    QLabel *m_topSliderLabel{};
    QLabel *m_bottomSliderLabel{};
    QCheckBox *m_diffsOnly{};
    QCheckBox *m_refCheck{};
    QPushButton *m_refBtn{};

    cv::Mat m_refMat;
    QPixmap m_refPix;

    void browseReference();
    void updateRefControls();

    cv::Mat m_leftMat;   // BGR, original size from disk (or resized right-side to match left)
    cv::Mat m_rightMat;
    bool m_pairIdentical{false};

    void updateNavLabel();
};