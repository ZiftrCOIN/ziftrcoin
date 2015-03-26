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

// The AMD chips that ZRC sgminer supports
static const string AMD_SPECIFIC_STRINGS[] = {
    "AMD",          "Radeon",       "Barts",        "BeaverCreek",      "Beaver Creek",     
    "Bonaire",      "Caicos",       "CapeVerde",    "Cape Verde",       "Cayman",  
    "Cedar",        "Cypress",      "Devastator",   "Hainan",           "Hawaii",
    "Iceland",      "Juniper",      "Kalindi",      "Loveland",         "Love Land",
    "Mullins",      "Oland",        "Pitcairn",     "Redwood",          "Scrapper", 
    "Spectre",      "Spooky",       "Tahiti",       "Tonga",            "Turks",
    "WinterPark",   "Winter Park",  "END"
};

static const string NVIDIA_SPECIFIC_STRINGS[] = {
    "Nvidia",     "GeForce",        "NV",           "Quadro",           "Tesla",
    "END"
};

static inline bool AContainsB(const QString& a, const QString& b)
{
    if (b.size() > a.size())
        return false;

    return a.contains(b, Qt::CaseInsensitive);
}

template <typename T>
static QString formatHashrate(T n)
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
    gpuMinerProcess->setProcessChannelMode(QProcess::SeparateChannels);

    readTimer = new QTimer(this);
    hashTimer = new QTimer(this);

    acceptedShares = 0;
    rejectedShares = 0;

    cpuInitThreads = 0;

    numGPUs = 6;

    QStringList list = QString(introText.c_str()).split("\n", QString::SkipEmptyParts);
    for (int i = 0; i < list.size(); i++) {
        this->AddListItem(list.at(i));
    }

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
    connect(hashTimer, SIGNAL(timeout()), this, SLOT(updateSpeed()));

    connect(ui->startButton, SIGNAL(pressed()), this, SLOT(startPressed()));
    connect(ui->clearButton, SIGNAL(pressed()), this, SLOT(clearPressed()));
    connect(ui->horizontalSlider, SIGNAL(valueChanged(int)), this, SLOT(changePercentMiningPower(int)));
    connect(ui->pokCheckBox, SIGNAL(toggled(bool)), this, SLOT(usePoKToggled(bool)));
    connect(ui->debugCheckBox, SIGNAL(toggled(bool)), this, SLOT(debugToggled(bool)));
    connect(ui->typeBox, SIGNAL(currentIndexChanged(int)), this, SLOT(typeChanged(int)));

    connect(cpuMinerProcess, SIGNAL(started()), this, SLOT(minerStarted()));
    connect(cpuMinerProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(minerError(QProcess::ProcessError)));
    connect(cpuMinerProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(minerFinished()));
    connect(cpuMinerProcess, SIGNAL(readyRead()), this, SLOT(readCPUMiningOutput()));

    connect(gpuMinerProcess, SIGNAL(started()), this, SLOT(minerStarted()));
    connect(gpuMinerProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(minerError(QProcess::ProcessError)));
    connect(gpuMinerProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(minerFinished()));
    connect(gpuMinerProcess, SIGNAL(readyRead()), this, SLOT(readGPUMiningOutput()));

    GPUState = GPU_UNINITIALIZED;
    this->LaunchGPUInitialCheck();

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

void MiningPage::LaunchGPUInitialCheck()
{
    if (this->GPUState == GPU_UNINITIALIZED)
    {
        this->GPUState = GPU_SETUP_LAUNCHED;

        QStringList args;
        args << "-n";

        QDir appDir = QDir(QCoreApplication::applicationDirPath());
        
        QString program = appDir.filePath("sgminer");
        if (!QFile::exists(program))
            program = "sgminer";

        gpuMinerProcess->start(program, args);
    }
}

void MiningPage::startPressed()
{
    int nPercentHashPow = ui->horizontalSlider->value();
    mapArgs["-usepercenthashpower"] = QString("%1").arg(nPercentHashPow).toUtf8().data();

    if (!minerActive)
    {
        // Start mining
        saveSettings();

        if (getMiningType() == ClientModel::SoloMining)
            minerStarted();
        else
            startPoolMining();
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

void MiningPage::startPoolMining()
{
    QStringList args;
    QString url = ui->serverLine->text();
    QString urlLine = QString("%1:%2").arg(url, ui->portLine->text());
    QString userLine = QString("%1").arg(ui->usernameLine->text());
    QString passwordLine = QString("%1").arg(ui->passwordLine->text());
    if (passwordLine.isEmpty())
        passwordLine = QString("x");

    // algorithm needs to be set in miner specific way
    args << "-o" << urlLine.toUtf8().data();
    args << "-u" << userLine.toUtf8().data();
    args << "-p" << passwordLine.toUtf8().data();

    QStringList gpuArgs(args);

    bool fStartCpuMining = ui->cpuCheckBox->isChecked();
    if (fStartCpuMining)
    {
        startCPUPoolMining(args);
    }

    bool fStartGpuMining = !GetCheckedGPUs().empty();
    if (fStartGpuMining)
    {
        startGPUPoolMining(gpuArgs);
    }

    if (fStartCpuMining || fStartCpuMining)
    {
        ui->mineSpeedLabel->setText("0 H/s");
        this->logShareCounts();

        readTimer->start(500);
    }
    else 
    {
        this->AddListItem("You do not have any devices enabled.");
    }

}

void MiningPage::startCPUPoolMining(QStringList args)
{
    unsigned int nPercentHashPow = GetArg("-usepercenthashpower", DEFAULT_USE_PERCENT_HASH_POWER);
    nPercentHashPow = std::min(std::max(nPercentHashPow, (unsigned int)0), (unsigned int)100);
    unsigned int nBestThreads = boost::thread::hardware_concurrency();
    cpuInitThreads = nPercentHashPow == 0 ? 0 : std::max(nBestThreads * nPercentHashPow / 100, (unsigned int)1);
    
    args << "-t" << QString("%1").arg(cpuInitThreads);
    args << "-a" << "zr5";
    args << "--retries" << "-1"; // Retry forever.
    args << "-P"; // This is needed for this to work correctly on Windows. Extra protocol dump helps flush the buffer quicker.

    threadSpeed.clear();

    acceptedShares = 0;
    rejectedShares = 0;

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
    if (useCuda)
    {
        args << "-a" << "zr5";
        for (int i = 0; i < numGPUs; i++)
        {
            if (!mapGpuCheckBoxesDisabled[1+i])
                args << "-d" << this->GetGPUCheckBox(1+i)->text();
        }
    }
    else 
    {
        args << "--algorithm" << "zr5";
        args << "-d" << GetCheckedGPUs().join(",");
        args << "-T";
    }

    // gpuSpeeds.clear();
    gpuSpeed = 0.0;

    QDir appDir = QDir(QCoreApplication::applicationDirPath());

    QString program = ""; // /Users/stephenmorse/Documents/GitHub/sgminer-ziftr/sgminer
    string base = useCuda ? "ccminer" : "sgminer";

    program = appDir.filePath(base.c_str());
    if (!QFile::exists(program))
        program = base.c_str();

    gpuMinerProcess->start(program, args);
    gpuMinerProcess->waitForStarted(-1);
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
    double totalGpuSpeed = 0;
    int gpuSpeedCount = 0;

    QByteArray outputBytes;
    outputBytes = gpuMinerProcess->readAllStandardOutput();

    QString outputString(outputBytes);

    if (!outputString.isEmpty())
    {
        QStringList list = outputString.split("\n", QString::SkipEmptyParts);
        for (int x = 0; x < list.size(); x++) {
            QString outputLine = list.at(x);

            // Ignore protocol dump
            if (!outputLine.startsWith("["))
                continue;

            if (ui->debugCheckBox->isChecked())
            {
                this->AddListItem(outputLine.trimmed());
            }
            ui->list->scrollToBottom();

            // Directly below is what guides the GPU states through the setup process

            static int gpuCounter = 0;
            static bool fFoundAmd = false;
            static bool fFoundnVidia = false;

            if (this->GPUState == GPU_SETUP_LAUNCHED)
            {
                if (outputLine.contains("Platform devices:"))
                {
                    this->GPUState = GPU_SETUP_DETECTED_GPU_COUNT;

                    int startNumGPUs = outputLine.indexOf("Platform devices:")+18;
                    QString numGPUsStr = outputLine.mid(startNumGPUs);

                    numGPUs = numGPUsStr.toInt();

                    for (int i = numGPUs; i < 6; i++)
                    {
                        if (this->GetGPULayout(1+i) != NULL)
                        {
                            QLayoutItem * item = NULL;
                            while ( ( item = this->GetGPULayout(1+i)->takeAt(0) ) != NULL )
                            {
                                delete item->widget();
                                delete item;
                            }
                            delete this->GetGPULayout(1+i);
                        }
                    }

                    gpuCounter = 0;
                    fFoundAmd = GetBoolArg("-useamd", false);
                    fFoundnVidia = GetBoolArg("-usecuda", false);
                    if (fFoundAmd && fFoundnVidia)
                    {
                        fFoundAmd = false;
                        fFoundnVidia = false;
                    }
                    continue;
                }
            }
            else if (this->GPUState == GPU_SETUP_DETECTED_GPU_COUNT)
            {
                if (outputLine.contains("max detected"))
                {
                    this->GPUState = GPU_SETUP_AWAITING_FIRST_EXIT;
                }
                else if (gpuCounter == numGPUs)
                {
                    // Avoid coming in here again
                    gpuCounter++;

                    // Default to false
                    useCuda = false;

                    if (fFoundnVidia)
                        useCuda = true;
                    else if (fFoundAmd)
                        useCuda = false;

                    continue;
                }
                else if (gpuCounter < numGPUs)
                {
                    gpuCounter++;

                    QStringList sublist = outputLine.split("\t", QString::SkipEmptyParts);
                    
                    QString gpuName = ""; 
                    if (sublist.size() > 2)
                        gpuName = sublist.at(2);

                    this->GetGPUCheckBox(gpuCounter)->setText(gpuName);

                    if (!fFoundnVidia && !fFoundAmd)
                    {
                        bool fSingleFoundnVidia = false;
                        for (int i = 0; !fSingleFoundnVidia && NVIDIA_SPECIFIC_STRINGS[i] != "END"; i++)
                        {
                            if (AContainsB(gpuName, QString(NVIDIA_SPECIFIC_STRINGS[i].c_str())))
                                fSingleFoundnVidia = true;
                        }

                        bool fSingleFoundAmd = false;
                        for (int i = 0; !fSingleFoundAmd && AMD_SPECIFIC_STRINGS[i] != "END"; i++)
                        {
                            if (AContainsB(gpuName, QString(AMD_SPECIFIC_STRINGS[i].c_str())))
                                fSingleFoundAmd = true;
                        }

                        if (fSingleFoundAmd == fSingleFoundnVidia)
                        {
                            // Shouldn't ever both be true, but if they were we'd want to disable this miner
                            mapGpuCheckBoxesDisabled[gpuCounter] = true;
                            this->GetGPUCheckBox(gpuCounter)->setEnabled(false);
                        }
                        else
                        {
                            // Use whichever one is found first
                            if (fSingleFoundnVidia)
                                fFoundnVidia = true;
                            if (fSingleFoundAmd)
                                fFoundAmd = true;
                        }

                    }

                }
            }

            // Don't parse miner output until have finished parsing "sgminer -n" output
            if (this->GPUState < GPU_SETUP_AWAITING_FIRST_EXIT)
                continue;

            // Now start parsing system specific outputs
            if (useCuda)
            {
                // The cuda output makes it easier as it will calculate the total of all gpu hashrate for us
                // eg [2015-03-19 13:27:40] Total: 220.35 khash/s

                // hash rate is reported like this
                // [2015-03-19 18:42:26] GPU #0: GeForce GT 650M, 211.28 khash/s
                int gpuIndex = outputLine.indexOf("GPU #");
                int gpuId = -1;
                if (gpuIndex > 0)
                {
                    // Get the gpu number
                    gpuIndex += 5;
                    int gpuEndIndex = outputLine.indexOf(":", gpuIndex);
                    QString gpuNumberString = outputLine.mid(gpuIndex, gpuEndIndex);

                    gpuId = gpuNumberString.toInt();
                }

                if (gpuId >= 0)
                {
                    gpuSpeeds[gpuId]
                }

                int hashrateIndex = outputLine.indexOf("Total:");
                if (hashrateIndex >= 0)
                {
                    //get everything between "Total: " and the next space " "
                    hashrateIndex += 7;
                    int endIndex = outputLine.indexOf(" ", hashrateIndex);
                    QString hashrateString = outputLine.mid(hashrateIndex, endIndex);

                    this->AddListItem(QString("GPU rate: ").append(hashrateString));

                    totalGpuSpeed += hashrateString.toDouble();
                    gpuSpeedCount++;
                }
            }
            else
            {
                // Use sgminer
                // this->AddListItem(outputLine.trimmed()); // TODO delete
                // ui->list->scrollToBottom();

            }

        }

        if (gpuSpeedCount > 0 && totalGpuSpeed > 0)
        {
            gpuSpeed = totalGpuSpeed / gpuSpeedCount; //get an average of the last few reported speeds
        }
    }
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

            if (!this->ProcessBasicLine(line))
            {
                if (line.contains("detected new block"))
                    reportToList("Detected a new block -- new round", NEW_ROUND, getTime(line));
                else if (line.contains("Supported options:"))
                    reportToList("Miner didn't start properly. Try checking your settings.", ERROR, NULL);
                else if (line.contains("The requested URL returned error: 403"))
                    reportToList("Couldn't connect. Please check your username and password.", ERROR, NULL);
                else if (line.contains("Connection refused"))
                    reportToList("Couldn't connect. Please check pool server and port.", ERROR, NULL);
                else if (line.contains("Stratum authentication failed"))
                    reportToList("Pool authentication failed. Retrying in 30 seconds.", ERROR, NULL);
                else if (line.contains("JSON-RPC call failed"))
                    reportToList("Couldn't communicate with server. Retrying in 30 seconds.", ERROR, NULL);
                else if (line.contains("thread ") && line.contains("khash/s"))
                {
                    int startThreadId = line.indexOf("thread ")+7;
                    int endThreadId = line.lastIndexOf(":");
                    QString threadIDstr = line.mid(startThreadId, endThreadId-startThreadId);

                    int threadID = threadIDstr.toInt();

                    threadSpeed[threadID] = this->ExtractHashRate(line);

                    updateSpeed();
                }
            }
        }
    }
}

bool MiningPage::ProcessBasicLine(QString line)
{
    if (line.contains("(yay!!!)"))
    {
        reportToList("Share accepted", SHARE_SUCCESS, getTime(line));
        return true;
    }
    else if (line.contains("(booooo)"))
    {
        reportToList("Share rejected", SHARE_FAIL, getTime(line));
        return true;
    }
    else
    {
        return false;
    }
}

double MiningPage::ExtractHashRate(QString line)
{
    int threadSpeedindx = line.indexOf(",");
    QString threadSpeedstr = line.mid(threadSpeedindx);
    threadSpeedstr.chop(8);
    threadSpeedstr.remove(", ");
    threadSpeedstr.remove(" ");
    threadSpeedstr.remove('\n');
    double speed = 0;
    return threadSpeedstr.toDouble();
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
    // If one process dies they both should quit
    if (cpuMinerProcess->state() != QProcess::NotRunning)
        cpuMinerProcess->kill();
    if (gpuMinerProcess->state() != QProcess::NotRunning)
        gpuMinerProcess->kill();

    if (getMiningType() == ClientModel::SoloMining)
        reportToList("Solo mining stopped.", ERROR, NULL);
    else if (this->GPUState == GPU_SETUP_AWAITING_FIRST_EXIT)
        this->GPUState = GPU_READY;
    else if (this->GPUState >= GPU_READY)
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
        else if (this->GPUState >= GPU_READY)
            reportToList("Miner started. You might not see any output for a few minutes.", STARTED, NULL);
    }
    minerActive = true;
    resetMiningButton();
    clientmodel->setMining(getMiningType(), true, -1);
}

void MiningPage::updateSpeed()
{
    qint64 NetworkHashrate = (qint64)GetNetworkHashPS(120, -1).get_int64();
    ui->networkHashRate->setText(QString("Network hash rate: %1").arg(formatHashrate(NetworkHashrate)));

    if (!minerActive)
    {
        ui->mineSpeedLabel->setText(QString("0 H/s"));
    }
    else if (this->getMiningType() == ClientModel::SoloMining)
    {
        qint64 Hashrate = GetBoolArg("-gen", false) && 
                GetArg("-usepercenthashpower", DEFAULT_USE_PERCENT_HASH_POWER) != 0 ? clientmodel->getHashrate() : 0;
        ui->mineSpeedLabel->setText(QString("%1").arg(formatHashrate(Hashrate)));

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
    else
    {
        double totalSpeed = 0;

        if (ui->cpuCheckBox->isChecked())
        {
            int totalThreads = 0;
            double totalCpuSpeed = 0;

            QMapIterator<int, double> iter(threadSpeed);
            while (iter.hasNext())
            {
                iter.next();
                totalCpuSpeed += iter.value();
                totalThreads++;
            }
            
            if (totalThreads != 0)
            {
                totalSpeed += (totalCpuSpeed * cpuInitThreads / totalThreads);
            }
        }

        if (!this->GetCheckedGPUs().empty())
        {
            totalSpeed += gpuSpeed;
        }

        // Everything is stored as a double of the number of kH/s, but formatted as whatever 
        // is most appropriate
        ui->mineSpeedLabel->setText(QString("%1").arg(formatHashrate(totalSpeed*1000)));
    }

    clientmodel->setMining(getMiningType(), minerActive, -1);
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
            updateSpeed();
            this->logShareCounts();
            break;

        case SHARE_FAIL:
            rejectedShares++;
            updateSpeed();
            this->logShareCounts();
            break;

        case NEW_ROUND:
            break;

        default:
            break;
    }

    ui->list->scrollToBottom();
}

void MiningPage::AddListItem(const QString& text)
{
    QListWidgetItem * item = new QListWidgetItem(text);
    item->setFlags(item->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
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

        for (int i = 0; i < numGPUs; i++)
        {
            this->GetGPUCheckBox(1+i)->setChecked(false);
        }
    }
    ui->pokCheckBox->setEnabled(type == ClientModel::SoloMining);

    ui->horizontalSlider->setEnabled(!minerActive || type == ClientModel::SoloMining);
    ui->serverLine->setEnabled(!minerActive);
    ui->portLine->setEnabled(!minerActive);
    ui->usernameLine->setEnabled(!minerActive);
    ui->passwordLine->setEnabled(!minerActive);

    ui->cpuCheckBox->setEnabled(!minerActive);

    if (minerActive || this->GPUState >= GPU_READY)
    {
        for (int i = 0; i < numGPUs; i++)
        {
            this->GetGPUCheckBox(1+i)->setEnabled(!minerActive && !mapGpuCheckBoxesDisabled[1+i] && type == ClientModel::PoolMining);
        }
    }
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

    QString messageTotal = QString("Total Shares Accepted: %1 - Rejected: %2").arg(acceptedString, rejectedString);
    this->reportToList(messageTotal, GENERIC, NULL);
    // this->AddListItem(messageTotal);
}

QStringList MiningPage::GetCheckedGPUs()
{
    QStringList ret;

    if (numGPUs > 0 && ui->gpu1CheckBox->isChecked())
        ret << QString("0");

    if (numGPUs > 1 && ui->gpu2CheckBox->isChecked())
        ret << QString("1");

    if (numGPUs > 2 && ui->gpu3CheckBox->isChecked())
        ret << QString("2");

    if (numGPUs > 3 && ui->gpu4CheckBox->isChecked())
        ret << QString("3");

    if (numGPUs > 4 && ui->gpu5CheckBox->isChecked())
        ret << QString("4");

    if (numGPUs > 5 && ui->gpu6CheckBox->isChecked())
        ret << QString("5");

    return ret;
}

QHBoxLayout * MiningPage::GetGPULayout(int nId)
{
    switch(nId)
    {
    case 1:  return ui->gpu1HorizontalLayout;
    case 2:  return ui->gpu2HorizontalLayout;
    case 3:  return ui->gpu3HorizontalLayout;
    case 4:  return ui->gpu4HorizontalLayout;
    case 5:  return ui->gpu5HorizontalLayout;
    case 6:  return ui->gpu6HorizontalLayout;
    default: return NULL;
    }
}

QCheckBox * MiningPage::GetGPUCheckBox(int nId)
{
    switch(nId)
    {
    case 1:  return ui->gpu1CheckBox;
    case 2:  return ui->gpu2CheckBox;
    case 3:  return ui->gpu3CheckBox;
    case 4:  return ui->gpu4CheckBox;
    case 5:  return ui->gpu5CheckBox;
    case 6:  return ui->gpu6CheckBox;
    default: return NULL;
    }
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
