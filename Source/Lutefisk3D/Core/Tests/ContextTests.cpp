#include <QTest>
#include "../Context.h"
class ContextTests : public QObject {
    Q_OBJECT
private slots:
    void verifyConstructionAndCheckState() {
        Urho3D::Context z;
        QVERIFY(z.GetEventDataMap().empty());
    }
};

QTEST_MAIN(ContextTests)
#include "ContextTests.moc"
