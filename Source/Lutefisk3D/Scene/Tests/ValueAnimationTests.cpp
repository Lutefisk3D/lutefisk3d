#include <QtTest/QTest>
#include "../Core/Context.h"
#include "../ValueAnimation.h"
class ValueAnimationTests : public QObject {
    Q_OBJECT
    Urho3D::Context *ctx;
private slots:
    void initTestCase()
    {
        ctx = new Urho3D::Context;
        Urho3D::ValueAnimation::RegisterObject(ctx);
    }
    void verifyConstructionAndCheckState() {
        Urho3D::ValueAnimation va(ctx);
        QCOMPARE(va.GetCategory(),QString(""));
    }
    void cleanupTestCase()
    {
        delete ctx;
    }
};

QTEST_MAIN(ValueAnimationTests)
#include "ValueAnimationTests.moc"
