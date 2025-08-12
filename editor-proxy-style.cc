// editor-proxy-style.cc
// Code for `editor-proxy-style` module.

#include "editor-proxy-style.h"                  // this module

#include <QStyleOptionViewItem>


EditorProxyStyle::EditorProxyStyle()
{}


EditorProxyStyle::~EditorProxyStyle()
{}


int EditorProxyStyle::pixelMetric(
  PixelMetric metric,
  const QStyleOption *option,
  const QWidget *widget) const
{
  if (metric == PM_MaximumDragDistance) {
    // The standard behavior is when the mouse is dragged too far away
    // from the scrollbar, it jumps back to its original position. I
    // find that behavior annoying and useless.  This disables it.
    return -1;
  }

  return QProxyStyle::pixelMetric(metric, option, widget);
}


int EditorProxyStyle::styleHint(
  StyleHint hint,
  QStyleOption const *option,
  QWidget const *widget,
  QStyleHintReturn *returnData) const
{
  if (hint == QStyle::SH_UnderlineShortcut) {
    // Always show the underlines on shortcuts.
    return 1;
  }

  return QProxyStyle::styleHint(hint, option, widget, returnData);
}


void EditorProxyStyle::drawControl(
  ControlElement element,
  const QStyleOption *option,
  QPainter *painter,
  const QWidget *widget) const
{
  // Prevent a mouse-hovered list item from being drawn differently,
  // which is confusing since it looks similar to the currently selected
  // item.  I want to do everything primarily with the keyboard without
  // visual interference depending on where the mouse cursor happens to
  // be.
  if (element == CE_ItemViewItem) {
    // Get the detailed options for this kind of item so that when we
    // make a copy we get all the required info.
    if (auto optViewItemSrc =
          qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
      // Make a copy of the options, with full info, so we can adjust
      // them.
      QStyleOptionViewItem opt(*optViewItemSrc);

      // Remove the mouse hover state so the item will draw the same
      // as if the mouse was not hovering on it.
      opt.state &= ~State_MouseOver;

      // Proceed with otherwise normal drawing.
      QProxyStyle::drawControl(element, &opt, painter, widget);
      return;
    }
  }

  // Completely normal drawing.
  QProxyStyle::drawControl(element, option, painter, widget);
}


// EOF
