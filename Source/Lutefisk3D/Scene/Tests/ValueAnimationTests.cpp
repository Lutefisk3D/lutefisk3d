#include <QTest>
#include "../Core/Context.h"
#include "../ValueAnimation.h"
class ValueAnimationTests : public QObject {
    Q_OBJECT
private slots:
    void verifyConstructionAndCheckState() {
        Urho3D::Context ctx;
        Urho3D::ValueAnimation va(&ctx);
        QVERIFY(va.GetCategory()=="z");
    }
};

QTEST_MAIN(ValueAnimationTests)
#include "ValueAnimationTests.moc"
