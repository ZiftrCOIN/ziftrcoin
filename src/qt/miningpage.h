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

// GPU state machine
enum GPU_STATE
{
    GPU_UNINITIALIZED,
    GPU_SETUP_LAUNCHED,
    GPU_SETUP_DETECTED_GPU_COUNT,
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

    bool minerActive;

    QProcess *cpuMinerProcess;
    QProcess *gpuMinerProcess;

    QMap<int, double> threadSpeed;
    double gpuSpeed;

    QTimer *readTimer;
    QTimer *hashTimer;

    int acceptedShares;
    int rejectedShares;

    int roundAcceptedShares;
    int roundRejectedShares;

    int cpuInitThreads;

    GPU_STATE GPUState;
    int numGPUs;
    QMap<int, bool> mapGpuCheckBoxStates;
    bool useCuda;

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
    void updateHashRates();

    QString getTime(QString);
    void EnableMiningControlsAppropriately();
    ClientModel::MiningType getMiningType();
    void typeChanged(int index);
    void usePoKToggled(bool checked);
    void debugToggled(bool checked);

    void changePercentMiningPower(int i);
    void startPressed();
    void clearPressed();

    void LaunchGPUInitialCheck();
    // void LoadGPUCheckBoxes();

private:
    Ui::MiningPage *ui;
    WalletModel *walletmodel;
    ClientModel *clientmodel;
    std::auto_ptr<WalletModel::UnlockContext> unlockContext;

    void resetMiningButton();
    void logShareCounts();
    void AddListItem(const QString& text);
    QStringList GetCheckedGPUs();
    QCheckBox * GetGPUCheckBox(int nId);

    // void restartMining(bool fGenerate);
    // void timerEvent(QTimerEvent *event);
    // void updateUI();
};

#endif // MININGPAGE_H
