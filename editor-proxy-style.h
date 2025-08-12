// editor-proxy-style.h
// `EditorProxyStyle`, Qt style overrides for the entire editor app.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_EDITOR_PROXY_STYLE_H
#define EDITOR_EDITOR_PROXY_STYLE_H

#include "editor-proxy-style-fwd.h"    // fwds for this module

#include <QProxyStyle>


// Define my look and feel overrides.
class EditorProxyStyle : public QProxyStyle {
public:
  EditorProxyStyle();
  virtual ~EditorProxyStyle() override;

  virtual int pixelMetric(
    PixelMetric metric,
    const QStyleOption *option = NULL,
    const QWidget *widget = NULL) const override;

  virtual int styleHint(
    StyleHint hint,
    QStyleOption const *option,
    QWidget const *widget,
    QStyleHintReturn *returnData) const override;

  virtual void drawControl(
    ControlElement element,
    const QStyleOption *option,
    QPainter *painter,
    const QWidget *widget) const override;
};


#endif // EDITOR_EDITOR_PROXY_STYLE_H
