#ifndef VELOTRACK_H
#define VELOTRACK_H

#include <QMainWindow>
#include <QVector>
#include <QSerialPort>
#include <cstdint>

class QPushButton;
class QCustomPlot;
class QComboBox;
class QLabel;
class QCheckBox;
class QSplashScreen;

class VeloTrackMainWindow: public QMainWindow
{
    Q_OBJECT
    
    enum Command
    {
        START_MEASUREMENT = 0,
        STOP_MEASUREMENT = 1,
        TRANSFER_BUFFER = 2,
        TRANSFER_TIME_RESOLUTION = 3,
    };

    enum Code
    {
        CONFIRM_MEASUREMENT_TAKEN = 0xCF,
        CONNECTION_ESTABLISHED = 0xCE,
    };

    QVector<double> d_data;
    QByteArray d_raw;
    QSerialPort *d_serial;
    uint16_t d_recordingInterval; // microseconds

    // GUI elements
    QComboBox   *d_portList;
    QPushButton *d_connectButton;
    QPushButton *d_scanButton;
    QPushButton *d_startButton;
    QPushButton *d_downloadButton;
    QCheckBox   *d_autoDownloadCheck;
    QCustomPlot *d_plot;
    QLabel      *d_status;
    QSplashScreen *d_splash;

public:
    VeloTrackMainWindow();
    QSplashScreen *getSplash() const;

private slots:
    void connectDevice();
    void disconnectDevice();
    void scanPorts();
    void sendStartSignal();
    void sendDownloadSignal();
    void saveData();
    void clearAll();

private:
    QByteArray requestData(Command command, size_t timeout = -1);
    void notifyError(QSerialPort::SerialPortError err);
    void processRaw();
    void updatePlot();
    void showErrorBox(QString const &title, QString const &msg) const;
    bool waitForDevice(Code code, int timeout);
};

inline QSplashScreen *VeloTrackMainWindow::getSplash() const
{
    return d_splash;
}

#endif
