// inputproxy.h
// interface that an editing mode can implement to define alternate
// handling of input events

#ifndef INPUTPROXY_H
#define INPUTPROXY_H

class EditorWidget;                    // editor-widget.h
class QKeyEvent;                       // qevent.h


// Pseudo-keys; operations that might be globally bound to
// a key but the effect might be something a proxy should
// handle.
enum InputPseudoKey {
  IPK_CANCEL,          // cancel current operation (Esc)

  NUM_INPUT_PSEUDO_KEYS
};


// main interface
class InputProxy {
protected:   // funcs
  virtual ~InputProxy();

public:      // funcs
  // handle a key press; return true if the proxy handled
  // the key, and therefore the editor should not do so
  // (default implementation: returns false)
  virtual bool keyPressEvent(QKeyEvent *k);

  // the editor to which we're attached is going away
  virtual void detach();

  // Handle a pseudo-key press.  Return true of handled,
  // false if not.  Default returns false.
  virtual bool pseudoKeyPress(InputPseudoKey pkey);
};


// convenience version that knows how to attach itself
class AttachInputProxy : public InputProxy {
protected:     // data
  // which editor we're attached to, if any
  EditorWidget *ed;           // (nullable serf)

public:      // funcs
  AttachInputProxy();
  virtual ~AttachInputProxy();

  // attach this proxy to an editor
  void attach(EditorWidget *ed);

  // detach from current editor
  virtual void detach();
};


#endif // INPUTPROXY_H
