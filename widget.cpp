#include "widget.h"
#include "ui_widget.h"

#include <QTimer>
#include <QMessageBox>
#include <QVariant>
#include <QCoreApplication>
#include <QProgressDialog>
#include <QTime>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QTextStream>

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

    connect(ui->btRun,SIGNAL(clicked()),this,SLOT(onRun()));
    connect(ui->btHold,SIGNAL(clicked()),this,SLOT(onHold()));
    connect(ui->btReset,SIGNAL(clicked()),this,SLOT(onReset()));

    // log file
    // set the name
    QDir logDir;
    if (!logDir.cd("log")) // if not existing create log dir
    {
        logDir.mkdir("log");
        logDir.cd("log");
    }
    QString fname(logDir.absolutePath());
    fname += QChar('/');
    fname += "stf16_";
    fname += QDate::currentDate().toString("ddMMyyyy");
    fname += ".log";

    logFile.setFileName(fname);
    QFile::OpenMode mode = logFile.exists() ? QFile::Append : QFile::Truncate;
    mode |= QIODevice::WriteOnly;
    logFile.open(mode);
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
    uint16_t v[8];
    modbus_t* ctx = (modbus_t*)ctx_;

    int T,Tsp,Twsp,W;

    // read registers 1 - 5
    addr = 1;
    if (modbus_read_input_registers(ctx,addr,5,v)==-1)
    {
        QMessageBox::critical(this,"Read failed due to modbus error",QString(modbus_strerror(errno)));
        return;
    }

    // temperature
    T = v[0];
    ui->lcdT->display(T);

    // target setpoint
    Tsp = v[1];
    ui->lcdTargetSetpoint->display(Tsp);

    // working setpoint
    Twsp = v[4];
    ui->lcdWorkingSetpoint->display(Twsp);

    // working output
    W = v[3];
    ui->lcdPower->display(0.1*W);

    // read register 23
    addr = 23;
    if (modbus_read_input_registers(ctx,addr,1,v)==-1)
    {
        QMessageBox::critical(this,"Read failed due to modbus error",QString(modbus_strerror(errno)));
        return;
    }

    QString msg;
    switch (v[0])
    {
    case 0: msg = "Reset"; break;
    case 1: msg = "Run  "; break;
    case 2: msg = "Hold "; break;
    case 3: msg = "End  "; break;
    }
    ui->edtStatus->setText(msg);

    updateStatus(v[0]);

    // read dwell timer 325 - 326(elapsed, remaining)
    addr = 325;
    if (modbus_read_input_registers(ctx,addr,2,v)==-1)
    {
        QMessageBox::critical(this,"Read failed due to modbus error",QString(modbus_strerror(errno)));
        return;
    }

    ui->edtTimeElapsed->setText(QString("%1 min").arg(v[0]));

    // read running cycle 332 - 333(elapsed)
    addr = 333;
    if (modbus_read_input_registers(ctx,addr,1,v)==-1)
    {
        QMessageBox::critical(this,"Read failed due to modbus error",QString(modbus_strerror(errno)));
        return;
    }

    ui->edtCycle->setText(QString("%1").arg(v[0]));

    // write to log file
    if (!logFile.isOpen()) return;

    QTextStream stream(&logFile);

    QDateTime cdt = QDateTime::currentDateTime();
    stream << cdt.toString("dd.MM.yyyy") << '\t'
           << cdt.toString("hh:mm:ss") << '\t'
           << msg << '\t'
           << T << '\t'
           << Tsp << '\t'
           << Twsp << '\t'
           << W*0.1 << '\n';
}

void Widget::updateStatus(int v)
{
    ui->btRun->setEnabled(v==0 || v==2);
    ui->btReset->setEnabled(v!=0);
    ui->btHold->setEnabled(v==1);
    ui->btDownload->setEnabled(v==0);
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


    for(int is=0; is<ns; is++)
    {
        for(int j=0; j<3; j++)
        {
            QTableWidgetItem* it = ui->tblProgram->item(is,j);
            it->setData(Qt::DisplayRole,0);
        }
    }
}

void Widget::onReset()
{
    const int addr = 23; // address of timer status T.STAT
    modbus_t* ctx = (modbus_t*)ctx_;

    if (modbus_write_register(ctx,addr,0)==-1) {
        QMessageBox::critical(this,"Error",
                              QString("Writing 0 to T.STAT(%1) failed with error %2").arg(addr).arg(modbus_strerror(errno)));
    }
}
void Widget::onRun()
{
    const int addr = 23; // address of timer status T.STAT
    modbus_t* ctx = (modbus_t*)ctx_;

    if (modbus_write_register(ctx,addr,1)==-1) {
        QMessageBox::critical(this,"Error",
                              QString("Writing 1 to T.STAT(%1) failed with error %2").arg(addr).arg(modbus_strerror(errno)));
    }
}
void Widget::onHold()
{
    const int addr = 23; // address of timer status T.STAT
    modbus_t* ctx = (modbus_t*)ctx_;

    if (modbus_write_register(ctx,addr,2)==-1) {
        QMessageBox::critical(this,"Error",
                              QString("Writing 2 to T.STAT(%1) failed with error %2").arg(addr).arg(modbus_strerror(errno)));
    }
}
