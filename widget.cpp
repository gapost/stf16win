#include "widget.h"
#include "ui_widget.h"

#include <QTimer>
#include <QMessageBox>

#include <modbus.h>

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget),
    ctx_(0)
{
    ui->setupUi(this);

    open();

    QTimer* t = new QTimer(this);
    connect(t,SIGNAL(timeout()),this,SLOT(cycle()));
    t->start(1000);
}

Widget::~Widget()
{
    delete ui;
}

bool Widget::open()
{
    const char device = "COM1";

    modbus_t* ctx = modbus_new_rtu(device, 9600, 'N', 8, 1);

    if (modbus_connect(ctx) == -1) {
        modbus_free(ctx);
        QMessageBox::critical(this,"Error","Could not connect to STF-16");
    }
    else {
        ctx_ = ctx;
    }

}

void Widget::cycle()
{
    if (!ctx_) return;

    int addr;
    uint16_t v;

    // temperature
    addr = 1;
    modbus_read_input_registers(ctx_,addr,1,&v);
    ui->lcdT->display((int)v);

    // setpoint
    addr = 24;
    modbus_read_input_registers(ctx_,addr,1,&v);
    ui->lcdTset->display((int)v);

    // output
    addr = 4;
    modbus_read_input_registers(ctx_,addr,1,&v);
    ui->lcdPower->display((int)v);


}
