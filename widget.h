#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QFile>

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

    void* ctx_; // modbus context

    bool block_cycle_; // flag to block update cycle

    QFile logFile;

    bool open();
    void updateStatus(int v);

private slots:
    void cycle();
    void upload();
    void download();
    void zeroout();
    void onRun();
    void onHold();
    void onReset();
};

#endif // WIDGET_H
