#pragma once
#include <QIcon>
#include <QColor>
QIcon icon(const QString &name, const QColor &color = QColor(), int size = 20);

QString platformIcon(const QString &system);
