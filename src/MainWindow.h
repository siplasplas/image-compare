#pragma once

#include <QMainWindow>
#include <QString>

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
    void compare();

private:
    void updateThumb(QLabel *label, const QString &path, QPixmap &cache);
    void rescaleAll();

    QPixmap m_leftPix;
    QPixmap m_rightPix;
    QPixmap m_diffPix;

    QLineEdit *m_leftEdit{};
    QLineEdit *m_rightEdit{};
    QPushButton *m_leftBtn{};
    QPushButton *m_rightBtn{};
    QPushButton *m_compareBtn{};
    QLabel *m_leftView{};
    QLabel *m_rightView{};
    QLabel *m_diffView{};
};