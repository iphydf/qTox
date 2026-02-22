/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "src/chatlog/chatlinecontentproxy.h"

#include <QGraphicsScene>
#include <QImage>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QWidget>
#include <QtTest/QtTest>

class TestChatLineContentProxy : public QObject
{
    Q_OBJECT
private slots:
    void testClippingOutOfBounds();
};

void TestChatLineContentProxy::testClippingOutOfBounds()
{
    auto* widget = new QWidget();
    widget->setFixedSize(100, 100);
    // Use a solid color background to reliably detect if the widget is drawn out of bounds.
    QPalette pal = widget->palette();
    pal.setColor(QPalette::Window, Qt::green);
    widget->setAutoFillBackground(true);
    widget->setPalette(pal);

    auto* proxy = new ChatLineContentProxy(widget, 100, 1.0f);

    QGraphicsScene scene;
    scene.addItem(proxy);

    QImage image(100, 100, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);

    // Simulate a strict clip set by a parent container (e.g., QGraphicsView viewport scroll).
    // Restrict drawing to only the bottom 50 pixels.
    painter.setClipRect(QRect(0, 50, 100, 50));

    // Call the proxy's paint method directly.
    QStyleOptionGraphicsItem option;
    option.exposedRect = QRectF(0, 0, 100, 105); // Expose the whole thing to force a draw
    proxy->paint(&painter, &option, nullptr);
    painter.end();

    // Check a pixel outside the clip region (e.g., at (50, 25)).
    // The proxy's painting behavior should not override the painter's clip region.
    // Since this area is clipped out, it should remain entirely transparent.
    const QColor pixelColor = image.pixelColor(50, 25);
    QCOMPARE(pixelColor.alpha(), 0);
}

QTEST_MAIN(TestChatLineContentProxy)
#include "chatlinecontentproxy_test.moc"
