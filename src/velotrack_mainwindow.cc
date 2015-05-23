#include "velotrack.ih"

VeloTrackMainWindow::VeloTrackMainWindow(): 
    QMainWindow(),
    d_data(100),
    d_serial(new QSerialPort(this)),
    d_recordingInterval(2000),
    d_portList(new QComboBox(this)),
    d_connectButton(new QPushButton("Connect", this)),
    d_scanButton(new QPushButton("Scan", this)),
    d_startButton(new QPushButton(this)),
    d_downloadButton(new QPushButton(this)),
    d_autoDownloadCheck(new QCheckBox("auto-download", this)),
    d_plot(new QCustomPlot(this)),
    d_status(new QLabel("VeloTrack v0.9", this)),
    d_splash(new QSplashScreen(this, QPixmap(":/img/splash.png"), Qt::WindowStaysOnTopHint))
{
    // Menubar
    QMenu *fileMenu = menuBar()->addMenu("&File");
    QAction *saveAction = fileMenu->addAction("&Save");
    QAction *clearAction = fileMenu->addAction("&Clear");
    QAction *quitAction = fileMenu->addAction("&Quit");

    saveAction->setShortcut(QKeySequence("Ctrl+S"));
    clearAction->setShortcut(QKeySequence("Ctrl+X"));
    quitAction->setShortcut(QKeySequence("Ctrl+Q"));

    QMenu *helpMenu = menuBar()->addMenu("&Help");
    QAction *aboutAction = helpMenu->addAction("&About");

    aboutAction->setShortcut(QKeySequence("F1"));

    // Configure the central widget layout:
    QWidget *centralWidget = new QWidget;
    QVBoxLayout *centralLayout = new QVBoxLayout(centralWidget);
    QHBoxLayout *connectLayout = new QHBoxLayout;
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    centralLayout->addLayout(connectLayout);
    centralLayout->addLayout(buttonLayout);
    this->setCentralWidget(centralWidget);

    // Add port-list and connect button
    connectLayout->addStretch();
    connectLayout->addWidget(d_portList, 5);
    connectLayout->addWidget(d_connectButton, 2);
    connectLayout->addWidget(d_scanButton, 2);
    connectLayout->addStretch();
    scanPorts();
    
    // Add buttons to the button-layout
    d_startButton->setFixedSize(80, 80);
    d_startButton->setIcon(QIcon(":/img/play.png"));
    d_startButton->setIconSize(d_startButton->size());

    d_downloadButton->setFixedSize(80, 80);
    d_downloadButton->setIcon(QIcon(":/img/download.png"));
    d_downloadButton->setIconSize(d_downloadButton->size());

    buttonLayout->addStretch();
    buttonLayout->addWidget(d_startButton);
    buttonLayout->addWidget(d_downloadButton);
    buttonLayout->addStretch();

    // Add checkbox to enable auto-download
    QHBoxLayout *checkboxLayout = new QHBoxLayout;
    checkboxLayout->addStretch();
    checkboxLayout->addWidget(d_autoDownloadCheck);
    checkboxLayout->addStretch();
    centralLayout->addLayout(checkboxLayout);
    d_autoDownloadCheck->setChecked(true);

    // Add plot window
    d_plot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    centralLayout->addWidget(d_plot);

    // Populate plot
    d_plot->addGraph();
    d_plot->xAxis->setLabel("time (s)");
    d_plot->yAxis->setLabel("speed (m/s)");
    d_plot->setBackground(centralWidget->palette().color(centralWidget->backgroundRole()));
    updatePlot();

    // Status textbox
    d_status->setAlignment(Qt::AlignCenter);
    centralLayout->addWidget(d_status);

    // Connections
    connect(saveAction, SIGNAL(triggered()), this, SLOT(saveData()));
    connect(clearAction, SIGNAL(triggered()), this, SLOT(clearAll()));
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), d_splash, SLOT(show()));

    connect(d_scanButton, SIGNAL(clicked()), this, SLOT(scanPorts()));
    connect(d_connectButton, SIGNAL(clicked()), this, SLOT(connectDevice()));
    connect(d_startButton, SIGNAL(clicked()), this, SLOT(sendStartSignal()));
    connect(d_downloadButton, SIGNAL(clicked()), this, SLOT(sendDownloadSignal()));
}

void VeloTrackMainWindow::showErrorBox(QString const &title, QString const &msg) const
{
    QMessageBox msgBox(QMessageBox::Critical, title, msg);
    msgBox.exec();
}

void VeloTrackMainWindow::saveData()
{
    QString fname = QFileDialog::getSaveFileName(this, "Save Data", "", "Text Files (*.txt)");
    if (fname.isEmpty())
        return;

    std::ofstream file(fname.toStdString());
    if (!file)
        return showErrorBox("Error Opening File", 
                            "VeloTrack was unable to open this file for writing. " 
                            "Make sure you have write permissions.");

    for (double val: d_data)
        file << val << '\n';
}


void VeloTrackMainWindow::clearAll()
{
    d_raw.clear();
    QVector<double> other(100);
    d_data.swap(other);
    updatePlot();
    d_status->setText("Cleared");
}


void VeloTrackMainWindow::updatePlot()
{
    // Generate x-data
    QVector<double> xData(d_data.size());
    for (int i = 0; i != xData.size(); ++i)
        xData[i] = (i + 1) * d_recordingInterval / 1e6;
    double xMax = xData[xData.size() - 1];

    // Plot
    d_plot->graph(0)->setData(xData, d_data);
    d_plot->xAxis->setRange(0, xMax);

    auto minmax = std::minmax_element(d_data.begin(), d_data.end());
    if (*minmax.first == 0 && *minmax.second == 0)
        d_plot->yAxis->setRange(-10, 10);
    else
        d_plot->yAxis->setRange(*minmax.first, *minmax.second);

    d_plot->xAxis->setTickStep(xMax / 10);

    d_plot->replot();
}

void VeloTrackMainWindow::scanPorts()
{
    d_portList->clear();

    QList<QSerialPortInfo> available = QSerialPortInfo::availablePorts();
    for (auto const &port: available)
        if (!port.description().isEmpty())
            d_portList->addItem(port.portName());
}

void VeloTrackMainWindow::notifyError(QSerialPort::SerialPortError err)
{
    static char const *messages[] =
        {
            /*  0 */ "No error.",
            /*  1 */ "An error occurred while attempting to open an non-existing device.",
            /*  2 */ "An error occurred while attempting to open an already opened device by another process or a user not having enough permission and credentials to open.",
            /*  3 */ "An error occurred while attempting to open an already opened device in this object.",
            /*  4 */ "Parity error detected by the hardware while reading data.",
            /*  5 */ "Framing error detected by the hardware while reading data.",
            /*  6 */ "Break condition detected by the hardware on the input line.",
            /*  7 */ "An I/O error occurred while writing the data.",
            /*  8 */ "An I/O error occurred while reading the data.",
            /*  9 */ "An I/O error occurred when a resource becomes unavailable, e.g. when the device is unexpectedly removed from the system.",
            /* 10 */ "The requested device operation is not supported or prohibited by the running operating system.",
            /* 11 */ "An unidentified error occurred.",
            /* 12 */ "A timeout error occurred.",
            /* 13 */ "Error while attempting an operation that can only be successfully performed if the device is open."
        };

    return showErrorBox("Error", messages[err]);
}

QByteArray VeloTrackMainWindow::requestData(Command command, size_t timeout)
{
    QByteArray result;

    QTimer timer(this);
    timer.setSingleShot(true);
    timer.setInterval(timeout);

    QEventLoop wait;
    auto conn1 = connect(d_serial, &QIODevice::readyRead, [&](){
            timer.start();
        });

    auto conn2 = connect(d_serial, &QIODevice::readyRead, [&](){
            while (d_serial->bytesAvailable())
                result.append(d_serial->readAll());
        });

    connect(&timer, &QTimer::timeout, [&](){
            wait.quit();
        });

    d_serial->write(QByteArray(1, command));
    wait.exec();

    disconnect(conn1);
    disconnect(conn2);
    return result;
}


void VeloTrackMainWindow::connectDevice()
{
    QString const &current = d_portList->currentText();
    if (current.isEmpty())
        return;

    d_serial->setPortName(current);
    d_serial->open(QIODevice::ReadWrite);

    QSerialPort::SerialPortError err = d_serial->error();
    d_serial->clearError();
    d_serial->setDataTerminalReady(true);

    if (err)
        return notifyError(err);

    disconnect(d_connectButton, SIGNAL(clicked()), this, SLOT(connectDevice()));
    connect(d_connectButton, SIGNAL(clicked()), this, SLOT(disconnectDevice()));

    d_status->setText("Connecting ...");
    if (waitForDevice(CONNECTION_ESTABLISHED, 10 * 1000))
    {
        QByteArray rec = requestData(TRANSFER_TIME_RESOLUTION, 500);
        d_recordingInterval = *reinterpret_cast<uint16_t*>(rec.data());
        
        d_connectButton->setText("Disconnect");
        d_status->setText("Connected");
    }
    else
    {
        d_status->setText("Connection Error");
        disconnectDevice();
    }
}

void VeloTrackMainWindow::disconnectDevice()
{
    d_serial->close();

    d_connectButton->setText("Connect");
    disconnect(d_connectButton, SIGNAL(clicked()), this, SLOT(disconnectDevice()));
    connect(d_connectButton, SIGNAL(clicked()), this, SLOT(connectDevice()));

    d_status->setText("Disconnected");
}

bool VeloTrackMainWindow::waitForDevice(Code code, int timeout)
{
    QEventLoop wait;
    connect(d_serial, SIGNAL(readyRead()), &wait, SLOT(quit()));
    if (timeout > 0)
        QTimer::singleShot(timeout, &wait, SLOT(quit()));

    wait.exec();
    return static_cast<unsigned char>(d_serial->read(1)[0]) == code;
}

void VeloTrackMainWindow::sendStartSignal()
{
    if (!d_serial->isOpen())
        return showErrorBox("No Connection", "Connect to the device first.");

    d_status->setText("Running measurement, please wait ...");

    d_serial->readAll(); // flush whatever is in the buffer
    d_serial->write(QByteArray(1, START_MEASUREMENT)); // issue the start-command
    if (waitForDevice(CONFIRM_MEASUREMENT_TAKEN, -1))
    {
        d_status->setText("Measurement finished");
        if (d_autoDownloadCheck->isChecked())
            sendDownloadSignal();
    }
    else
        d_status->setText("Error: could not confirm succesful measurement");
}

void VeloTrackMainWindow::sendDownloadSignal()
{
    if (!d_serial->isOpen())
        return showErrorBox("No Connection", "Connect to the device first.");

    d_status->setText("Downloading data, please wait ...");
    d_raw = requestData(TRANSFER_BUFFER, 1000);
    d_status->setText("Download finished");
    
    processRaw();
    saveData();
}

void VeloTrackMainWindow::processRaw()
{
    d_data.clear();

    int16_t const *rawInts = reinterpret_cast<int16_t*>(d_raw.data());
    double const conversionConstant = 7.45e-5; // m per step
    int const n = d_raw.size() / 2;

    d_data.reserve(n);
    for (int i = 0; i != n; ++i)
        d_data.push_back(rawInts[i] * conversionConstant * 1e6 / d_recordingInterval);

    updatePlot();
}
