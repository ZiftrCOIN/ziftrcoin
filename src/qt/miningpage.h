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

#include "clientmodel.h"
#include "walletmodel.h"
#include "optionsmodel.h"

class ClientModel;
class WalletModel;

// Log types
#define STARTED 0
#define SHARE_SUCCESS 1
#define SHARE_FAIL 2
#define ERROR 3
#define NEW_ROUND 4

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

    QProcess *minerProcess;

    QMap<int, double> threadSpeed;

    QTimer *readTimer;
    QTimer *hashTimer;

    int acceptedShares;
    int rejectedShares;

    int roundAcceptedShares;
    int roundRejectedShares;

    int initThreads;

    OptionsModel *optionsModel;

    void setClientModel(ClientModel *model);
    void setOptionsModel(OptionsModel *optionsModel);

public slots:
    void startPoolMining();
    void stopPoolMining();

    void updateSpeed();

    void loadSettings();
    void loadSettingsFromFile();
    void saveSettings();
    void saveSettingsToFile();

    void reportToList(QString, int, QString);

    void minerStarted();

    void minerError(QProcess::ProcessError);
    void minerFinished();

    void readProcessOutput();
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

private:
    Ui::MiningPage *ui;
    WalletModel *walletmodel;
    ClientModel *clientmodel;
    std::auto_ptr<WalletModel::UnlockContext> unlockContext;

    void resetMiningButton();
    void logShareCounts();
    void AddListItem(const QString& text);

    // void restartMining(bool fGenerate);
    // void timerEvent(QTimerEvent *event);
    // void updateUI();
};

#endif // MININGPAGE_H
