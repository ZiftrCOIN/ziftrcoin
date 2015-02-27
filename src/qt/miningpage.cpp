#include "miningpage.h"
#include "ui_miningpage.h"
#include "main.h"
#include "util.h"
#include "rpcserver.h"
#include "walletmodel.h"

#include <boost/thread.hpp>
#include <stdio.h>

extern json_spirit::Value GetNetworkHashPS(int lookup, int height);

MiningPage::MiningPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MiningPage)
{
    ui->setupUi(this);

    ui->sliderCores->setMinimum(0);
    ui->sliderCores->setMaximum(100);
    ui->sliderCores->setValue(50);
    ui->labelNCores->setText(QString("%1").arg(50));

    connect(ui->sliderCores, SIGNAL(valueChanged(int)), this, SLOT(changeNumberOfCores(int)));
    connect(ui->pushSwitchMining, SIGNAL(clicked()), this, SLOT(switchMining()));

    updateUI();
    startTimer(1500);
}

MiningPage::~MiningPage()
{
    delete ui;
}

void MiningPage::setModel(WalletModel *model)
{
    this->model = model;
}

void MiningPage::updateUI()
{
    unsigned int nPercentHashPow = GetArg("-usepercenthashpower", 50);
    nPercentHashPow = std::min(std::max(nPercentHashPow, (unsigned int)0), (unsigned int)100);

    ui->labelNCores->setText(QString("%1").arg(nPercentHashPow));
    ui->pushSwitchMining->setText(GetBoolArg("-gen", false)? tr("Stop mining") : tr("Start mining"));
}

void MiningPage::restartMining(bool fGenerate)
{
    int nPercentHashPow = ui->sliderCores->value();

    mapArgs["-usepercenthashpower"] = QString("%1").arg(nPercentHashPow).toUtf8().data();

    // unlock wallet before mining
    if (fGenerate && !unlockContext.get())
    {
        this->unlockContext.reset(new WalletModel::UnlockContext(model->requestUnlock()));
        if (!unlockContext->isValid())
        {
            unlockContext.reset(NULL);
            return;
        }
    }

    json_spirit::Array Args;
    Args.push_back(fGenerate);
    setgenerate(Args, false);

    // lock wallet after mining
    if (!fGenerate)
        unlockContext.reset(NULL);

    updateUI();
}

void MiningPage::changeNumberOfCores(int i)
{
    restartMining(GetBoolArg("-gen", false));
}

void MiningPage::switchMining()
{
    restartMining(!GetBoolArg("-gen", false));
}

static QString formatTimeInterval(CBigNum t)
{
    enum  EUnit { YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, NUM_UNITS };

    const int SecondsPerUnit[NUM_UNITS] =
    {
        31556952, // average number of seconds in gregorian year
        31556952/12, // average number of seconds in gregorian month
        24*60*60, // number of seconds in a day
        60*60, // number of seconds in an hour
        60, // number of seconds in a minute
        1
    };

    const char* UnitNames[NUM_UNITS] =
    {
        "year",
        "month",
        "day",
        "hour",
        "minute",
        "second"
    };

    if (t > 0xFFFFFFFF)
    {
        t /= SecondsPerUnit[YEAR];
        return QString("%1 years").arg(t.ToString(10).c_str());
    }
    else
    {
        unsigned int t32 = t.getuint();

        int Values[NUM_UNITS];
        for (int i = 0; i < NUM_UNITS; i++)
        {
            Values[i] = t32/SecondsPerUnit[i];
            t32 %= SecondsPerUnit[i];
        }

        int FirstNonZero = 0;
        while (FirstNonZero < NUM_UNITS && Values[FirstNonZero] == 0)
            FirstNonZero++;

        QString TimeStr;
        for (int i = FirstNonZero; i < std::min(FirstNonZero + 3, (int)NUM_UNITS); i++)
        {
            int Value = Values[i];
            TimeStr += QString("%1 %2%3 ").arg(Value).arg(UnitNames[i]).arg((Value == 1)? "" : "s"); // FIXME: this is English specific
        }
        return TimeStr;
    }
}

static QString formatHashrate(int64_t n)
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

void MiningPage::timerEvent(QTimerEvent *)
{
    int64_t NetworkHashrate = GetNetworkHashPS(120, -1).get_int64();
    int64_t Hashrate = GetBoolArg("-gen", false) && GetArg("-usepercenthashpower", 50) != 0 ? gethashespersec(json_spirit::Array(), false).get_int64() : 0;

    QString NextBlockTime;
    if (Hashrate == 0 || GetArg("-usepercenthashpower", 50) == 0)
        NextBlockTime = QChar(L'∞');
    else
    {
        CBigNum Target;
        Target.SetCompact(GetNextWorkRequired(chainActive.Tip(), GetTime()));
        CBigNum ExpectedTime = (CBigNum(1) << 256)/(Target*Hashrate);
        NextBlockTime = formatTimeInterval(ExpectedTime);
    }

    ui->labelNethashrate->setText(formatHashrate(NetworkHashrate));
    ui->labelYourHashrate->setText(formatHashrate(Hashrate));
    ui->labelNextBlock->setText(NextBlockTime);
}
