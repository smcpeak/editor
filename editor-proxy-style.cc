// editor-proxy-style.cc
// Code for `editor-proxy-style` module.

#include "editor-proxy-style.h"        // this module

#include "smqtutil/qstringb.h"         // qstringb

#include "smbase/sm-trace.h"           // INIT_TRACE, etc.

#include <QApplication>
#include <QStyleOptionViewItem>


INIT_TRACE("editor-proxy-style");


// ------------------------- EditorProxyStyle --------------------------
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


// --------------------------- global funcs ----------------------------
void installEditorStyleSheet(QApplication &app)
{
  // Get the application font.
  QFontInfo fi(QApplication::font());
  int sz = fi.pixelSize();
  TRACE1("font info pixel size: " << sz);

  // *******************************************************************
  // NOTE: This style sheet stuff seems flaky.  I should try to make any
  // future customizations using `EditorProxyStyle`, and in fact ideally
  // remove the customizations below in favor of the proxy.
  // *******************************************************************

  // Set the scrollbars to have a darker thumb.  Otherwise this is
  // meant to imitate the Windows 10 scrollbars.  (That is just for
  // consistency with other apps; I don't think the design is good.)
  //
  // Changing the color of the thumb requires basically re-implementing
  // the entire scrollbar visuals, unfortunately.  This specification is
  // based on the examples in the Qt docs at:
  //
  //   http://doc.qt.io/qt-5/stylesheet-examples.html#customizing-qscrollbar
  //
  // but I then modified it quite a bit.
  //
  // The sz+1 and sz-1 stuff is because the default font on Windows
  // seems to have a pixel size of 16, and I originally had used sizes
  // of 15 and 17 in this style sheet, so I've now adjusted it so the
  // sizes should scale with the initial font, but be the same as they
  // were when using the default font.
  std::string borderColor("#C0C0C0");
  app.setStyleSheet(qstringb(""
    "QScrollBar:vertical {"
    "  background: white;"
    "  width: "<<(sz+1)<<"px;"
    "  margin: "<<(sz+1)<<"px 0 "<<(sz+1)<<"px 0;" // top left right bottom?
    "}"
    "QScrollBar::handle:vertical {"
    "  border: 1px solid #404040;"
    "  background: #808080;"
    "  min-height: 20px;"
    "}"
    "QScrollBar::add-line:vertical {"
    "  border: 1px solid " << borderColor << ";"
    "  background: white;"
    "  height: "<<(sz+1)<<"px;"
    "  subcontrol-position: bottom;"
    "  subcontrol-origin: margin;"
    "}"
    "QScrollBar::sub-line:vertical {"
    "  border: 1px solid " << borderColor << ";"
    "  background: white;"
    "  height: "<<(sz+1)<<"px;"
    "  subcontrol-position: top;"
    "  subcontrol-origin: margin;"
    "}"
    "QScrollBar::up-arrow:vertical {"
    // This border-image trick causes the image to be stretched to fill
    // the available space, whereas with just 'image' it would always
    // be the original 15x15 size.
    //
    // The images themselves are made available to Qt by compiling
    // `resources.qrc` with the Qt `rcc` tool and linking that into the
    // executable.
    "  border-image: url(:/pix/scroll-up-arrow.png) 0 0 0 0 stretch stretch;"
    "  width: "<<(sz-1)<<"px;"
    "  height: "<<(sz-1)<<"px;"
    "}"
    "QScrollBar::down-arrow:vertical {"
    "  border-image: url(:/pix/scroll-down-arrow.png) 0 0 0 0 stretch stretch;"
    "  width: "<<(sz-1)<<"px;"
    "  height: "<<(sz-1)<<"px;"
    "}"
    "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
    "  border-left: 1px solid " << borderColor << ";"
    "  border-right: 1px solid " << borderColor << ";"
    "  background: none;"
    "}"

    /* Adjust the color of items in list views that are selected but the
       list does not have focus.  The default is a light gray that is
       almost indistinguishable from the zebra color, such that it is
       very hard to see what is selected.  This color is much more
       distinct.

       The default color for an item that is selected, while the list
       view has focus, is #0078D7. */
    "MyTableWidgetxx:item:selected:!active {"
    "  background: #88CCEE;"
    "}"
  ));
}


// EOF
