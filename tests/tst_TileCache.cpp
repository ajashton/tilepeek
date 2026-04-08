#include "map/TileCache.h"

#include <QTest>

class TestTileCache : public QObject {
    Q_OBJECT

private slots:
    void insertAndGet()
    {
        TileCache cache(10);
        QPixmap pm(256, 256);
        pm.fill(Qt::red);

        cache.insert({0, 0, 0}, pm);
        QCOMPARE(cache.size(), 1);

        auto result = cache.get({0, 0, 0});
        QVERIFY(result.has_value());
        QCOMPARE(result->size(), QSize(256, 256));
    }

    void getMissing()
    {
        TileCache cache(10);
        QVERIFY(!cache.get({0, 0, 0}).has_value());
    }

    void lruEviction()
    {
        TileCache cache(3);
        QPixmap pm(1, 1);

        cache.insert({0, 0, 0}, pm);
        cache.insert({0, 1, 0}, pm);
        cache.insert({0, 2, 0}, pm);
        QCOMPARE(cache.size(), 3);

        // Insert a 4th — should evict the LRU (0,0,0)
        cache.insert({0, 3, 0}, pm);
        QCOMPARE(cache.size(), 3);
        QVERIFY(!cache.get({0, 0, 0}).has_value());
        QVERIFY(cache.get({0, 1, 0}).has_value());
        QVERIFY(cache.get({0, 2, 0}).has_value());
        QVERIFY(cache.get({0, 3, 0}).has_value());
    }

    void getPromotesToMru()
    {
        TileCache cache(3);
        QPixmap pm(1, 1);

        cache.insert({0, 0, 0}, pm);
        cache.insert({0, 1, 0}, pm);
        cache.insert({0, 2, 0}, pm);

        // Access the oldest entry to promote it
        cache.get({0, 0, 0});

        // Insert a 4th — should evict (0,1,0) which is now the LRU
        cache.insert({0, 3, 0}, pm);
        QVERIFY(cache.get({0, 0, 0}).has_value());
        QVERIFY(!cache.get({0, 1, 0}).has_value());
    }

    void insertOverwrite()
    {
        TileCache cache(3);
        QPixmap pm1(1, 1);
        pm1.fill(Qt::red);
        QPixmap pm2(2, 2);
        pm2.fill(Qt::blue);

        cache.insert({0, 0, 0}, pm1);
        cache.insert({0, 0, 0}, pm2);
        QCOMPARE(cache.size(), 1);

        auto result = cache.get({0, 0, 0});
        QVERIFY(result.has_value());
        QCOMPARE(result->size(), QSize(2, 2));
    }

    void clearRemovesAll()
    {
        TileCache cache(10);
        QPixmap pm(1, 1);

        cache.insert({0, 0, 0}, pm);
        cache.insert({1, 0, 0}, pm);
        cache.clear();
        QCOMPARE(cache.size(), 0);
        QVERIFY(!cache.get({0, 0, 0}).has_value());
    }

    void evictOtherZooms()
    {
        TileCache cache(10);
        QPixmap pm(1, 1);

        cache.insert({0, 0, 0}, pm);
        cache.insert({1, 0, 0}, pm);
        cache.insert({1, 1, 0}, pm);
        cache.insert({2, 0, 0}, pm);
        QCOMPARE(cache.size(), 4);

        cache.evictOtherZooms(1);
        QCOMPARE(cache.size(), 2);
        QVERIFY(!cache.get({0, 0, 0}).has_value());
        QVERIFY(cache.get({1, 0, 0}).has_value());
        QVERIFY(cache.get({1, 1, 0}).has_value());
        QVERIFY(!cache.get({2, 0, 0}).has_value());
    }
};

QTEST_MAIN(TestTileCache)
#include "tst_TileCache.moc"
