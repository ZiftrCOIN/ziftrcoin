#ifndef MININGPAGE_H
#define MININGPAGE_H

#include <QWidget>
#include <memory>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTime>
#include <QTimer>
#include <QStringList>
#include <QMap>
#include <QSettings>
#include <QCheckBox>
#include <QtGui>
#include <QHBoxLayout>

#include "clientmodel.h"
#include "walletmodel.h"

class ClientModel;
class WalletModel;

// Log types
#define STARTED 0
#define SHARE_SUCCESS 1
#define SHARE_FAIL 2
#define ERROR 3
#define NEW_ROUND 4
#define GENERIC 5

// GPU state machine
enum GPU_STATE
{
    GPU_UNINITIALIZED,
    GPU_SETUP_LAUNCHED,
    GPU_SETUP_DETECTED_GPU_COUNT,
    GPU_SETUP_AWAITING_FIRST_EXIT,
    GPU_READY
};

namespace Ui {
class MiningPage;
}

class MiningPage : public QWidget
{
    Q_OBJECT

public:
    explicit MiningPage(QWidget *parent = 0);
    ~MiningPage();

    void setWalletModel(WalletModel *model);

    bool fHaveMinerd;
    bool fHaveSgminer;
    bool fHaveCcminer;

    bool minerActive;

    QProcess *cpuMinerProcess;
    QProcess *gpuMinerProcess;

    QMap<int, double> threadSpeed;
    QMap<int, double> gpuSpeeds;

    QTimer *readTimer;
    QTimer *hashTimer;

    int acceptedShares;
    int rejectedShares;

    int roundAcceptedShares;

    int cpuInitThreads;

    GPU_STATE GPUState;
    QMap<int, bool> mapGpuCheckBoxesDisabled;
    int numGPUs;
    bool useCuda;

    bool uiSetRpcUser;
    bool uiSetRpcPassword;

    void setClientModel(ClientModel *model);

public slots:
    void startPoolMining();
    void startCPUPoolMining(QStringList args);
    void startGPUPoolMining(QStringList args);

    void stopPoolMining();

    void updateSpeed();

    void loadSettings();
    void saveSettings();

    void reportToList(QString, int, QString);

    void minerStarted();

    void minerError(QProcess::ProcessError);
    void minerFinished();

    void readCPUMiningOutput();
    void readGPUMiningOutput();

    QString getTime(QString);
    void EnableMiningControlsAppropriately();
    ClientModel::MiningType getMiningType();
    void typeChanged(int index);
    void usePoKToggled(bool checked);
    void debugToggled(bool checked);
    void CPUCheckBoxToggled(bool checked);
    void GPUCheckBoxToggled(bool checked);

    void changePercentMiningPower(int i);
    void startPressed();
    void clearPressed();

    void LaunchGPUInitialCheck();

private:
    Ui::MiningPage *ui;
    WalletModel *walletmodel;
    ClientModel *clientmodel;
    std::auto_ptr<WalletModel::UnlockContext> unlockContext;

    void resetMiningButton();
    void logShareCounts();
    void AddListItem(const QString& text);
    QStringList GetCheckedGPUs();
    QHBoxLayout * GetGPULayout(int nId);
    QCheckBox * GetGPUCheckBox(int nId);
    bool ProcessBasicLine(QString line);
    double ExtractHashRate(QString line);
    void SetDefaultServerConfigs();
    void DeleteGPUBoxesAbove(int n);

    // void restartMining(bool fGenerate);
    // void timerEvent(QTimerEvent *event);
    // void updateUI();
};

#endif // MININGPAGE_H
