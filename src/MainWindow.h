#pragma once

#include <QMainWindow>
#include <QString>
#include <QVector>
#include <utility>

class QLineEdit;
class QLabel;
class QPushButton;

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
};