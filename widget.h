#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>


namespace Ui {
class Widget;
}

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = 0);
    ~Widget();

private:
    Ui::Widget *ui;

    void* ctx_;

    bool open();

private slots:
    void cycle();
    void upload();
    void download();
    void zeroout();
};

#endif // WIDGET_H