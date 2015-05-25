#include "widget.h"
#include "ui_widget.h"

#include <QTimer>
#include <QMessageBox>
#include <QVariant>
#include <QCoreApplication>
#include <QProgressDialog>
#include <QTime>

#include <modbus.h>

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget),
    ctx_(0),
    block_cycle_(false)
{
    ui->setupUi(this);

    // insert widgets in every cell in program matrix
    for(int i=0; i<24; i++)
    {
        ui->tblProgram->setItem(i/3,i%3,new QTableWidgetItem());
    }

    // connect to furnace by modbus
    open();

    // start update timer
    QTimer* t = new QTimer(this);
    connect(t,SIGNAL(timeout()),this,SLOT(cycle()));
    t->start(1000);

    connect(ui->btDownload,SIGNAL(clicked()),this,SLOT(download()));
    connect(ui->btUpload,SIGNAL(clicked()),this,SLOT(upload()));
    connect(ui->btZeroout,SIGNAL(clicked()),this,SLOT(zeroout()));
}

Widget::~Widget()
{
    delete ui;
}

// connect to furnace by modbus
bool Widget::open()
{
    const char* device = "COM1";

    modbus_t* ctx = modbus_new_rtu(device, 9600, 'N', 8, 1);

    if (modbus_connect(ctx) == -1) {
        modbus_free(ctx);
        QMessageBox::critical(this,"Error","Could not connect to STF-16");
    }
    else {
        ctx_ = ctx;
        modbus_set_slave(ctx,1);
    }

    return ctx_!=0;

}

void Widget::cycle()
{
    if (!ctx_) return;

    int addr;
    uint16_t v;
    modbus_t* ctx = (modbus_t*)ctx_;

    // temperature
    addr = 1;
    modbus_read_input_registers(ctx,addr,1,&v);
    ui->lcdT->display((int)v);

    // setpoint
    addr = 24;
    modbus_read_input_registers(ctx,addr,1,&v);
    ui->lcdTset->display((int)v);

    // output
    addr = 4;
    modbus_read_input_registers(ctx,addr,1,&v);
    ui->lcdPower->display((int)v);

}

void Widget::upload()
{
    if (!ctx_) {
        QMessageBox::warning(this,"Error","Not connected to STF-16");
        return;
    }

    const int ns = 8; // 8 steps
    const int nt = 8*3; // 3 parameters per step
    uint16_t v[nt];
    const int addr = 1280; // starting address
    modbus_t* ctx = (modbus_t*)ctx_;

    if (modbus_read_input_registers(ctx,addr,nt,v)==-1) {
        QMessageBox::critical(this,"Upload failed due to modbus error",QString(modbus_strerror(errno)));
        return;
    }

    for(int is=0; is<ns; is++)
    {
        for(int j=0; j<3; j++)
        {
            QTableWidgetItem* it = ui->tblProgram->item(is,j);
            it->setData(Qt::DisplayRole,(int)v[is*3+j]);
        }
    }

}

void Widget::download()
{
    if (!ctx_) {
        QMessageBox::warning(this,"Error","Not connected to STF-16");
        return;
    }

    const int ns = 8; // 8 steps
    const int nt = 8*3; // 3 parameters per step
    uint16_t v[nt];
    const int addr = 1280; // starting address
    modbus_t* ctx = (modbus_t*)ctx_;

    QProgressDialog progress("Sending parameters...", "Abort", 0, nt, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    block_cycle_ = true;

    for(int is=0; is<ns; is++)
    {
        for(int j=0; j<3; j++)
        {
            progress.setValue(is*3 + j + 1);

            if (progress.wasCanceled())
                break;

            QTableWidgetItem* it = ui->tblProgram->item(is,j);
            //v[is*3+j] = it->data(Qt::DisplayRole).toInt();

            int reg = addr + is*3 +j;
            if (modbus_write_register(ctx,reg,it->data(Qt::DisplayRole).toInt())==-1) {
                QMessageBox::critical(this,"Error",
                                      QString("Download of %1 failed with error %2").arg(reg).arg(modbus_strerror(errno)));
                return;
            }

            // wait 1 s
            QTime t;
            t.start();
            while(t.elapsed()<300)
                QCoreApplication::processEvents(QEventLoop::AllEvents,50);


        }
    }

     block_cycle_ = false;

    /*if (modbus_write_registers(ctx,addr,nt,v)==-1) {
        QMessageBox::critical(this,"Download failed due to modbus error",QString(modbus_strerror(errno)));
    }*/

}

void Widget::zeroout()
{
    const int ns = 8; // 8 steps
    const int nt = 8*3; // 3 parameters per step


    for(int is=0; is<ns; is++)
    {
        for(int j=0; j<3; j++)
        {
            QTableWidgetItem* it = ui->tblProgram->item(is,j);
            it->setData(Qt::DisplayRole,0);
        }
    }
}
