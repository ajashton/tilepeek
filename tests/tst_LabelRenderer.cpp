// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#include "map/LabelRenderer.h"

#include <QTest>
#include <vector>

class TestLabelRenderer : public QObject {
    Q_OBJECT

private slots:
    void selectNonCollidingKeepsDisjoint()
    {
        std::vector<QRectF> boxes{
            QRectF(0, 0, 10, 10),
            QRectF(20, 20, 10, 10),
            QRectF(40, 40, 10, 10),
        };
        auto accepted = LabelRenderer::selectNonColliding(boxes);
        QCOMPARE(accepted.size(), std::size_t{3});
        QCOMPARE(accepted[0], std::size_t{0});
        QCOMPARE(accepted[1], std::size_t{1});
        QCOMPARE(accepted[2], std::size_t{2});
    }

    void selectNonCollidingDropsOverlap()
    {
        std::vector<QRectF> boxes{
            QRectF(0, 0, 10, 10),
            QRectF(5, 5, 10, 10),   // overlaps with #0
            QRectF(100, 100, 10, 10),
        };
        auto accepted = LabelRenderer::selectNonColliding(boxes);
        QCOMPARE(accepted.size(), std::size_t{2});
        QCOMPARE(accepted[0], std::size_t{0});
        QCOMPARE(accepted[1], std::size_t{2});
    }

    void selectNonCollidingOrderDependent()
    {
        // Two overlapping boxes: whichever is first wins.
        std::vector<QRectF> boxes1{
            QRectF(0, 0, 10, 10),
            QRectF(5, 5, 10, 10),
        };
        auto a1 = LabelRenderer::selectNonColliding(boxes1);
        QCOMPARE(a1.size(), std::size_t{1});
        QCOMPARE(a1[0], std::size_t{0});

        std::vector<QRectF> boxes2{
            QRectF(5, 5, 10, 10),
            QRectF(0, 0, 10, 10),
        };
        auto a2 = LabelRenderer::selectNonColliding(boxes2);
        QCOMPARE(a2.size(), std::size_t{1});
        QCOMPARE(a2[0], std::size_t{0});
    }

    void selectNonCollidingEmpty()
    {
        std::vector<QRectF> boxes;
        auto accepted = LabelRenderer::selectNonColliding(boxes);
        QVERIFY(accepted.empty());
    }
};

QTEST_GUILESS_MAIN(TestLabelRenderer)
#include "tst_LabelRenderer.moc"
