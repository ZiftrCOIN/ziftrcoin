#include "uritests.h"

#include "guiutil.h"
#include "walletmodel.h"

#include <QUrl>

void URITests::uriTests()
{
    SendCoinsRecipient rv;
    QUrl uri;
    uri.setUrl(QString("ziftrcoin:ZDHj78ZXr9vB6EPs7bfqLGTw7HFNxnj2xw?req-dontexist="));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("ziftrcoin:ZDHj78ZXr9vB6EPs7bfqLGTw7HFNxnj2xw?dontexist="));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ZDHj78ZXr9vB6EPs7bfqLGTw7HFNxnj2xw"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("ziftrcoin:ZDHj78ZXr9vB6EPs7bfqLGTw7HFNxnj2xw?label=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ZDHj78ZXr9vB6EPs7bfqLGTw7HFNxnj2xw"));
    QVERIFY(rv.label == QString("Wikipedia Example Address"));
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("ziftrcoin:ZDHj78ZXr9vB6EPs7bfqLGTw7HFNxnj2xw?amount=0.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ZDHj78ZXr9vB6EPs7bfqLGTw7HFNxnj2xw"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100);

    uri.setUrl(QString("ziftrcoin:ZDHj78ZXr9vB6EPs7bfqLGTw7HFNxnj2xw?amount=1.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ZDHj78ZXr9vB6EPs7bfqLGTw7HFNxnj2xw"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100100);

    uri.setUrl(QString("ziftrcoin:ZDHj78ZXr9vB6EPs7bfqLGTw7HFNxnj2xw?amount=100&label=Wikipedia Example"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ZDHj78ZXr9vB6EPs7bfqLGTw7HFNxnj2xw"));
    QVERIFY(rv.amount == 10000000LL);
    QVERIFY(rv.label == QString("Wikipedia Example"));

    uri.setUrl(QString("ziftrcoin:ZDHj78ZXr9vB6EPs7bfqLGTw7HFNxnj2xw?message=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("ZDHj78ZXr9vB6EPs7bfqLGTw7HFNxnj2xw"));
    QVERIFY(rv.label == QString());

    QVERIFY(GUIUtil::parseBitcoinURI("ziftrcoin:ZDHj78ZXr9vB6EPs7bfqLGTw7HFNxnj2xw?message=Wikipedia Example Address", &rv));
    QVERIFY(rv.address == QString("ZDHj78ZXr9vB6EPs7bfqLGTw7HFNxnj2xw"));
    QVERIFY(rv.label == QString());

    uri.setUrl(QString("ziftrcoin:ZDHj78ZXr9vB6EPs7bfqLGTw7HFNxnj2xw?req-message=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("ziftrcoin:ZDHj78ZXr9vB6EPs7bfqLGTw7HFNxnj2xw?amount=1,000&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("ziftrcoin:ZDHj78ZXr9vB6EPs7bfqLGTw7HFNxnj2xw?amount=1,000.0&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));
}
