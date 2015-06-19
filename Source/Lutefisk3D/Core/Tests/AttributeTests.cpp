#include <QTest>
#include "../Attribute.h"
#include "../Variant.h"
class AttributeTests : public QObject {
    Q_OBJECT
private slots:
    void verifyConstructionAndCheckState() {
        Urho3D::AttributeInfo z;
        QVERIFY(z.name_.isEmpty());
        QVERIFY(Urho3D::VAR_NONE == z.type_);
    }
    void verifyCopy() {
        Urho3D::AttributeInfo z;
        //VariantType type, const char* name, AttributeAccessor* accessor, const Variant& defaultValue, unsigned mode
        Urho3D::AttributeInfo filled(Urho3D::VAR_INTRECT,"Tester",nullptr,Urho3D::Variant(Urho3D::IntRect(0,0,100,200)),1);
        z = filled;
        QVERIFY(z.type_==Urho3D::VAR_INTRECT);
        QVERIFY(z.name_=="Tester");
        QVERIFY(z.offset_==0);
        QVERIFY(z.enumNames_==nullptr);
        QVERIFY(z.defaultValue_==Urho3D::IntRect(0,0,100,200));
        QVERIFY(z.ptr_==nullptr);
    }
};

QTEST_MAIN(AttributeTests)
#include "AttributeTests.moc"
