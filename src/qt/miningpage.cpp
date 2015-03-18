#include "miningpage.h"
#include "ui_miningpage.h"
#include "main.h"
#include "util.h"
#include "bitcoingui.h"
#include "rpcserver.h"
#include "walletmodel.h"

#include <boost/thread.hpp>
#include <stdio.h>

extern json_spirit::Value GetNetworkHashPS(int lookup, int height);

static const string introText = 
    "Welcome to ziftrCOIN mining!\n"
    "If all you would like to do is to just mine to the ziftrCOIN pool, \n"
    "then just click 'Start Mining'!\n"
    "\n"
    "Please occasionally check that the cumulative hashrate of the pool you are \n"
    "in does not have more than 50\% of the network hashrate. It is not healty \n"
    "for the hashing power of the network to be so concentrated. \n"
    "\n"
    "You can also set the following parameters in your ziftrcoin.conf file to set \n"
    "and automatically load your chosen pool configurations at start. \n"
    "  poolserver=\n"
    "  poolport=\n"
    "  poolusername=\n"
    "  poolpassword=\n\n";

static QString formatHashrate(qint64 n)
{
    if (n == 0)
        return "0 H/s";

    int i = (int)floor(log(n)/log(1000));
    float v = n*pow(1000.0f, -i);

    QString prefix = "";
    if (i >= 1 && i < 9)
        prefix = " kMGTPEZY"[i];

    return QString("%1 %2H/s").arg(v, 0, 'f', 2).arg(prefix);
}

MiningPage::MiningPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MiningPage)
{
    ui->setupUi(this);

    setFixedSize(400, 420);

    minerActive = false;

    ui->horizontalSlider->setMinimum(0);
    ui->horizontalSlider->setMaximum(100);
    ui->horizontalSlider->setValue(100);
    ui->labelPercentHR->setText(QString("%1").arg(100));

    cpuMinerProcess = new QProcess(this);
    cpuMinerProcess->setProcessChannelMode(QProcess::MergedChannels);

    gpuMinerProcess = new QProcess(this);
    gpuMinerProcess->setProcessChannelMode(QProcess::MergedChannels);

    readTimer = new QTimer(this);
    hashTimer = new QTimer(this);

    acceptedShares = 0;
    rejectedShares = 0;

    roundAcceptedShares = 0;
    roundRejectedShares = 0;

    initThreads = 0;

    this->AddListItem(QString(introText.c_str()));

    ui->serverLine->setText(QString(GetArg("-poolserver", "stratum+tcp://ziftrpool.io").c_str()));
    ui->portLine->setText(QString(GetArg("-poolport", "3032").c_str()));
    
    string sPoolUsername = GetArg("-poolusername", "");
    
    if (sPoolUsername.empty())
    {
        // If getaccountaddress fails due to not having enough addresses in key pool,
        // just don't autofill
        try {
            json_spirit::Array params;
            params.push_back(string("Ziftr Pool Payouts"));
            params.push_back(true);
            sPoolUsername = getaccountaddress(params, false).get_str();
        } 
        catch (exception& e) {}
    }
    ui->usernameLine->setText(QString(sPoolUsername.c_str()));

    ui->passwordLine->setText(QString(GetArg("-poolpassword", "").c_str()));

    connect(readTimer, SIGNAL(timeout()), this, SLOT(readProcessOutput()));
    connect(hashTimer, SIGNAL(timeout()), this, SLOT(updateHashRates()));

    connect(ui->startButton, SIGNAL(pressed()), this, SLOT(startPressed()));
    connect(ui->clearButton, SIGNAL(pressed()), this, SLOT(clearPressed()));
    connect(ui->horizontalSlider, SIGNAL(valueChanged(int)), this, SLOT(changePercentMiningPower(int)));
    connect(ui->pokCheckBox, SIGNAL(toggled(bool)), this, SLOT(usePoKToggled(bool)));
    connect(ui->debugCheckBox, SIGNAL(toggled(bool)), this, SLOT(debugToggled(bool)));
    connect(ui->typeBox, SIGNAL(currentIndexChanged(int)), this, SLOT(typeChanged(int)));

    connect(cpuMinerProcess, SIGNAL(started()), this, SLOT(minerStarted()));
    connect(cpuMinerProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(minerError(QProcess::ProcessError)));
    connect(cpuMinerProcess, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(minerFinished()));
    connect(cpuMinerProcess, SIGNAL(readyRead()), this, SLOT(readCPUMiningOutput()));

    connect(gpuMinerProcess, SIGNAL(started()), this, SLOT(minerStarted()));
    connect(gpuMinerProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(minerError(QProcess::ProcessError)));
    connect(gpuMinerProcess, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(minerFinished()));
    connect(gpuMinerProcess, SIGNAL(readyRead()), this, SLOT(readGPUMiningOutput()));

    hashTimer->start(1500);
}

MiningPage::~MiningPage()
{
    cpuMinerProcess->kill();
    gpuMinerProcess->kill();
    delete ui;
}

void MiningPage::setWalletModel(WalletModel *model)
{
    this->walletmodel = model;
}

void MiningPage::setClientModel(ClientModel *model)
{
    this->clientmodel = model;
    loadSettings();
}

void MiningPage::startPressed()
{
    int nPercentHashPow = ui->horizontalSlider->value();
    mapArgs["-usepercenthashpower"] = QString("%1").arg(nPercentHashPow).toUtf8().data();

    if (minerActive == false)
    {
        // Start mining
        saveSettings();

        if (getMiningType() == ClientModel::SoloMining)
            minerStarted();
        else
        {
            //startPoolMining();
            //TODO checkbox for mining types
            startPoolMining(true, true);

        }
    }
    else
    {
        // Stop mining
        if (getMiningType() == ClientModel::SoloMining)
            minerFinished();
        else
            stopPoolMining();
    }
}

void MiningPage::clearPressed()
{
    ui->list->clear();
}

void MiningPage::startPoolMining(bool useCpu, bool useGpu)
{

    QStringList args;
    QString url = ui->serverLine->text();
    QString urlLine = QString("%1:%2").arg(url, ui->portLine->text());
    QString userLine = QString("%1").arg(ui->usernameLine->text());
    QString passwordLine = QString("%1").arg(ui->passwordLine->text());
    if (passwordLine.isEmpty())
        passwordLine = QString("x");

    args << "-a" << "zr5";
    args << "-o" << urlLine.toUtf8().data();
    args << "-u" << userLine.toUtf8().data();
    args << "-p" << passwordLine.toUtf8().data();

    QStringList gpuArgs(args);
    if(useCpu)
    {
        startCPUPoolMining(args);
    }

    if(useGpu)
    {
        startGPUPoolMining(gpuArgs);
    }


    ui->mineSpeedLabel->setText("Your hash rate: N/A");
    this->logShareCounts();

    readTimer->start(500);

}

void MiningPage::startCPUPoolMining(QStringList args)
{
    unsigned int nPercentHashPow = GetArg("-usepercenthashpower", DEFAULT_USE_PERCENT_HASH_POWER);
    nPercentHashPow = std::min(std::max(nPercentHashPow, (unsigned int)0), (unsigned int)100);
    unsigned int nBestThreads = boost::thread::hardware_concurrency();
    initThreads = nPercentHashPow == 0 ? 0 : std::max(nBestThreads * nPercentHashPow / 100, (unsigned int)1);
    args << "-t" << QString("%1").arg(initThreads);

    args << "--retries" << "-1"; // Retry forever.
    args << "-P"; // This is needed for this to work correctly on Windows. Extra protocol dump helps flush the buffer quicker.

    threadSpeed.clear();

    acceptedShares = 0;
    rejectedShares = 0;

    roundAcceptedShares = 0;
    roundRejectedShares = 0;

    // If minerd is in current path, then use that. Otherwise, assume minerd is in the path somewhere.
    QDir appDir = QDir(QCoreApplication::applicationDirPath());
    QString program = appDir.filePath("minerd");
    if (!QFile::exists(program))
        program = "minerd";

    if (ui->debugCheckBox->isChecked())
    {
        this->AddListItem(QString("Using minerd application located at: ").append(program));
    }

    cpuMinerProcess->start(program, args);
    cpuMinerProcess->waitForStarted(-1);


}


void MiningPage::startGPUPoolMining(QStringList args)
{

}


void MiningPage::stopPoolMining()
{
    cpuMinerProcess->kill();
    gpuMinerProcess->kill();
    readTimer->stop();
}

void MiningPage::saveSettings()
{
    clientmodel->setDebug(ui->debugCheckBox->isChecked());
    clientmodel->setMiningServer(ui->serverLine->text());
    clientmodel->setMiningPort(ui->portLine->text());
    clientmodel->setMiningUsername(ui->usernameLine->text());
    clientmodel->setMiningPassword(ui->passwordLine->text());
}

void MiningPage::loadSettings()
{
    ui->debugCheckBox->setChecked(clientmodel->getDebug());
    if (ui->serverLine->text().isEmpty())
        ui->serverLine->setText(clientmodel->getMiningServer());
    if (ui->portLine->text().isEmpty())
        ui->portLine->setText(clientmodel->getMiningPort());
    if (ui->usernameLine->text().isEmpty())
        ui->usernameLine->setText(clientmodel->getMiningUsername());
    if (ui->passwordLine->text().isEmpty())
        ui->passwordLine->setText(clientmodel->getMiningPassword());

    ui->horizontalSlider->setValue(GetArg("-usepercenthashpower", DEFAULT_USE_PERCENT_HASH_POWER));
    ui->typeBox->setCurrentIndex(ui->serverLine->text().isEmpty() ? 0 : 1);
}



void MiningPage::readGPUMiningOutput()
{

}

void MiningPage::readCPUMiningOutput()
{
    QByteArray output;

    cpuMinerProcess->reset();

    output = cpuMinerProcess->readAll();

    QString outputString(output);

    if (!outputString.isEmpty())
    {
        QStringList list = outputString.split("\n", QString::SkipEmptyParts);
        int i;
        for (i=0; i<list.size(); i++)
        {
            QString line = list.at(i);

            // Ignore protocol dump
            if (!line.startsWith("[") || line.contains("JSON protocol") || line.contains("HTTP hdr"))
                continue;

            if (ui->debugCheckBox->isChecked())
            {
                this->AddListItem(line.trimmed());
            }
            ui->list->scrollToBottom();

            if (line.contains("(yay!!!)"))
                reportToList("Share accepted", SHARE_SUCCESS, getTime(line));
            else if (line.contains("(booooo)"))
                reportToList("Share rejected", SHARE_FAIL, getTime(line));
            else if (line.contains("detected new block"))
                reportToList("Detected a new block -- new round", NEW_ROUND, getTime(line));
            else if (line.contains("Supported options:"))
                reportToList("Miner didn't start properly. Try checking your settings.", ERROR, NULL);
            else if (line.contains("The requested URL returned error: 403"))
                reportToList("Couldn't connect. Please check your username and password.", ERROR, NULL);
            else if (line.contains("Connection refused"))
                reportToList("Couldn't connect. Please check pool server and port.", ERROR, NULL);
            else if (line.contains("JSON-RPC call failed"))
                reportToList("Couldn't communicate with server. Retrying in 30 seconds.", ERROR, NULL);
            else if (line.contains("thread ") && line.contains("khash/s"))
            {
                int startThreadId = line.indexOf("thread ")+7;
                int endThreadId = line.lastIndexOf(":");
                QString threadIDstr = line.mid(startThreadId, endThreadId-startThreadId);

                int threadID = threadIDstr.toInt();

                int threadSpeedindx = line.indexOf(",");
                QString threadSpeedstr = line.mid(threadSpeedindx);
                threadSpeedstr.chop(8);
                threadSpeedstr.remove(", ");
                threadSpeedstr.remove(" ");
                threadSpeedstr.remove('\n');
                double speed=0;
                speed = threadSpeedstr.toDouble();

                threadSpeed[threadID] = speed;

                updateSpeed();
            }
        }
    }
}

void MiningPage::updateHashRates()
{
    qint64 NetworkHashrate = (qint64)GetNetworkHashPS(120, -1).get_int64();
    ui->networkHashRate->setText(QString("Network hash rate: %1").arg(formatHashrate(NetworkHashrate)));

    if (!minerActive)
    {
        ui->mineSpeedLabel->setText(QString("Your hash rate: 0 H/s"));
    }
    else if (this->getMiningType() == ClientModel::SoloMining)
    {
        qint64 Hashrate = GetBoolArg("-gen", false) && GetArg("-usepercenthashpower", DEFAULT_USE_PERCENT_HASH_POWER) != 0 ? clientmodel->getHashrate() : 0;
        ui->mineSpeedLabel->setText(QString("Your hash rate: %1").arg(formatHashrate(Hashrate)));

        // QString NextBlockTime;
        // if (Hashrate == 0 || GetArg("-usepercenthashpower", DEFAULT_USE_PERCENT_HASH_POWER) == 0)
        //     NextBlockTime = QChar(L'∞');
        // else
        // {
        //     CBigNum Target;
        //     Target.SetCompact(GetNextWorkRequired(chainActive.Tip(), GetTime()));
        //     CBigNum ExpectedTime = (CBigNum(1) << 256)/(Target*Hashrate);
        //     NextBlockTime = formatTimeInterval(ExpectedTime);
        // }
        // ui->labelNextBlock->setText(NextBlockTime);
    }
}

void MiningPage::minerError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart)
    {
        reportToList("Miner failed to start. Make sure you have the minerd executable and libraries in the same directory as ZiftrCOIN-Qt.", ERROR, NULL);
    }
}

void MiningPage::minerFinished()
{
    if (getMiningType() == ClientModel::SoloMining)
        reportToList("Solo mining stopped.", ERROR, NULL);
    else
        reportToList("Miner exited.", ERROR, NULL);
    this->AddListItem("");
    minerActive = false;
    resetMiningButton();
    clientmodel->setMining(getMiningType(), false, -1);
}

void MiningPage::minerStarted()
{
    if (!minerActive)
    {
        if (getMiningType() == ClientModel::SoloMining)
            reportToList("Solo mining started.", ERROR, NULL);
        else
            reportToList("Miner started. You might not see any output for a few minutes.", STARTED, NULL);
    }
    minerActive = true;
    resetMiningButton();
    clientmodel->setMining(getMiningType(), true, -1);
}

void MiningPage::updateSpeed()
{
    double totalSpeed=0;
    int totalThreads=0;

    QMapIterator<int, double> iter(threadSpeed);
    while(iter.hasNext())
    {
        iter.next();
        totalSpeed += iter.value();
        totalThreads++;
    }

    if (totalThreads == 0)
        return;

    // If all threads haven't reported the hash speed yet, make an assumption
    if (totalThreads != initThreads)
    {
        totalSpeed = (totalSpeed * initThreads / totalThreads);
    }

    QString speedString = QString("%1").arg(totalSpeed);

    if (totalThreads == initThreads)
        ui->mineSpeedLabel->setText(QString("Your hash rate: %1 kH/s").arg(speedString));
    else
        ui->mineSpeedLabel->setText(QString("Your hash rate: ~%1 kH/s").arg(speedString));

    clientmodel->setMining(getMiningType(), true, -1);
}

void MiningPage::reportToList(QString msg, int type, QString time)
{
    QString message;
    if (time == NULL)
        message = QString("[%1] - %2").arg(QTime::currentTime().toString(), msg);
    else
        message = QString("[%1] - %2").arg(time, msg);

    this->AddListItem(message);

    switch(type)
    {
        case SHARE_SUCCESS:
            acceptedShares++;
            roundAcceptedShares++;
            updateSpeed();
            this->logShareCounts();
            break;

        case SHARE_FAIL:
            rejectedShares++;
            roundRejectedShares++;
            updateSpeed();
            this->logShareCounts();
            break;

        case NEW_ROUND:
            roundAcceptedShares = 0;
            roundRejectedShares = 0;
            break;

        default:
            break;
    }


    ui->list->scrollToBottom();
}

void MiningPage::AddListItem(const QString& text)
{
    QListWidgetItem * item = new QListWidgetItem(text);
    item->setFlags(item->flags() | Qt::ItemIsSelectable);
    ui->list->addItem(item);
}

// Function for fetching the time
QString MiningPage::getTime(QString time)
{
    if (time.contains("["))
    {
        time.resize(21);
        time.remove("[");
        time.remove("]");
        time.remove(0,11);

        return time;
    }
    else
        return NULL;
}

void MiningPage::EnableMiningControlsAppropriately()
{
    ClientModel::MiningType type = this->getMiningType();

    ui->typeBox->setEnabled(!minerActive);

    if (type == ClientModel::PoolMining)
    {
        ui->pokCheckBox->setChecked(false);
    }
    else
    {
        ui->pokCheckBox->setChecked(GetBoolArg("-usepok", DEFAULT_USE_POK));
    }
    ui->pokCheckBox->setEnabled(type == ClientModel::SoloMining);

    ui->horizontalSlider->setEnabled(!minerActive || type == ClientModel::SoloMining);
    ui->serverLine->setEnabled(!minerActive);
    ui->portLine->setEnabled(!minerActive);
    ui->usernameLine->setEnabled(!minerActive);
    ui->passwordLine->setEnabled(!minerActive);
}

ClientModel::MiningType MiningPage::getMiningType()
{
    if (ui->typeBox->currentIndex() == 0)  // Solo Mining
    {
        return ClientModel::SoloMining;
    }
    else if (ui->typeBox->currentIndex() == 1)  // Pool Mining
    {
        return ClientModel::PoolMining;
    }
    return ClientModel::SoloMining;
}

void MiningPage::typeChanged(int index)
{
    EnableMiningControlsAppropriately();

}

void MiningPage::usePoKToggled(bool checked)
{
    if (this->getMiningType() == ClientModel::SoloMining)
        mapArgs["-usepok"] = (checked ? "1" : "0");
}

void MiningPage::debugToggled(bool checked)
{
    clientmodel->setDebug(checked);
}

void MiningPage::changePercentMiningPower(int i)
{
    mapArgs["-usepercenthashpower"] = QString("%1").arg(i).toUtf8().data();
    ui->labelPercentHR->setText(QString("%1").arg(i));
    // restartMining(GetBoolArg("-gen", false));
}

void MiningPage::resetMiningButton()
{
    ui->startButton->setText(minerActive ? "Stop Mining" : "Start Mining");
    QString style;
    if (minerActive)
        style = "QPushButton { color: #e46e1f; }";
    else 
        style = "QPushButton { color: #15444A; }";
    ui->startButton->setStyleSheet(style);
    EnableMiningControlsAppropriately();
}

void MiningPage::logShareCounts()
{
    QString acceptedString = QString("%1").arg(acceptedShares);
    QString rejectedString = QString("%1").arg(rejectedShares);

    QString roundAcceptedString = QString("%1").arg(roundAcceptedShares);
    QString roundRejectedString = QString("%1").arg(roundRejectedShares);

    QString messageTotal = QString("Total Shares Accepted: %1 - Rejected: %2").arg(acceptedString, rejectedString);
    QString messageShare = QString("Round Shares Accepted: %1 - Rejected: %2").arg(roundAcceptedString, roundRejectedString);
    this->AddListItem(messageTotal);
    this->AddListItem(messageShare);
}

// static QString formatTimeInterval(CBigNum t)
// {
//     enum  EUnit { YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, NUM_UNITS };

//     const int SecondsPerUnit[NUM_UNITS] =
//     {
//         31556952, // average number of seconds in gregorian year
//         31556952/12, // average number of seconds in gregorian month
//         24*60*60, // number of seconds in a day
//         60*60, // number of seconds in an hour
//         60, // number of seconds in a minute
//         1
//     };

//     const char* UnitNames[NUM_UNITS] =
//     {
//         "year",
//         "month",
//         "day",
//         "hour",
//         "minute",
//         "second"
//     };

//     if (t > 0xFFFFFFFF)
//     {
//         t /= SecondsPerUnit[YEAR];
//         return QString("%1 years").arg(t.ToString(10).c_str());
//     }
//     else
//     {
//         unsigned int t32 = t.getuint();

//         int Values[NUM_UNITS];
//         for (int i = 0; i < NUM_UNITS; i++)
//         {
//             Values[i] = t32/SecondsPerUnit[i];
//             t32 %= SecondsPerUnit[i];
//         }

//         int FirstNonZero = 0;
//         while (FirstNonZero < NUM_UNITS && Values[FirstNonZero] == 0)
//             FirstNonZero++;

//         QString TimeStr;
//         for (int i = FirstNonZero; i < std::min(FirstNonZero + 3, (int)NUM_UNITS); i++)
//         {
//             int Value = Values[i];
//             TimeStr += QString("%1 %2%3 ").arg(Value).arg(UnitNames[i]).arg((Value == 1)? "" : "s"); // FIXME: this is English specific
//         }
//         return TimeStr;
//     }
// }
