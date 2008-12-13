// qtutil.cc
// code for qtutil.h

#include "qtutil.h"      // this module

#include <qevent.h>      // QKeyEvent


static void handleFlag(stringBuilder &sb, Qt::ButtonState &state,
                       Qt::ButtonState flag, char const *flagName)
{
  if (state & flag) {
    if (sb.length() > 0) {
      sb << "+";
    }
    sb << flagName;
    state = (Qt::ButtonState)(state & ~flag);
  }
}


string toString(Qt::ButtonState state)
{
  stringBuilder sb;

  #define HANDLE_FLAG(flag) \
    handleFlag(sb, state, Qt::flag, #flag) /* user ; */

  HANDLE_FLAG(LeftButton);
  HANDLE_FLAG(RightButton);
  HANDLE_FLAG(MidButton);
  HANDLE_FLAG(ShiftButton);
  HANDLE_FLAG(ControlButton);
  HANDLE_FLAG(AltButton);
  HANDLE_FLAG(Keypad);         // ?

  #undef HANDLE_FLAG

  if (state) {
    if (sb.length() > 0) {
      sb << " (plus unknown flags)";
    }
    else {
      sb << "(unknown flags)";
    }
  }

  if (sb.length() == 0) {
    sb << "NoButton";
  }

  return sb;
}


char const *toString(Qt::Key k)
{
  // this list adapted from the one in qt/src/kernel/qnamespace.h

  #define HANDLE_KEY(key) \
    case Qt::key: return #key;

  switch (k) {
    // misc keys
    HANDLE_KEY(Key_Escape)
    HANDLE_KEY(Key_Tab)
    HANDLE_KEY(Key_Backtab)
    HANDLE_KEY(Key_Backspace)
    HANDLE_KEY(Key_Return)
    HANDLE_KEY(Key_Enter)
    HANDLE_KEY(Key_Insert)
    HANDLE_KEY(Key_Delete)
    HANDLE_KEY(Key_Pause)
    HANDLE_KEY(Key_Print)
    HANDLE_KEY(Key_SysReq)

    // cursor movement
    HANDLE_KEY(Key_Home)
    HANDLE_KEY(Key_End)
    HANDLE_KEY(Key_Left)
    HANDLE_KEY(Key_Up)
    HANDLE_KEY(Key_Right)
    HANDLE_KEY(Key_Down)
    HANDLE_KEY(Key_PageUp)
    HANDLE_KEY(Key_PageDown)

    // modifiers
    HANDLE_KEY(Key_Shift)
    HANDLE_KEY(Key_Control)
    HANDLE_KEY(Key_Meta)
    HANDLE_KEY(Key_Alt)
    HANDLE_KEY(Key_CapsLock)
    HANDLE_KEY(Key_NumLock)
    HANDLE_KEY(Key_ScrollLock)

    // function keys
    HANDLE_KEY(Key_F1)
    HANDLE_KEY(Key_F2)
    HANDLE_KEY(Key_F3)
    HANDLE_KEY(Key_F4)
    HANDLE_KEY(Key_F5)
    HANDLE_KEY(Key_F6)
    HANDLE_KEY(Key_F7)
    HANDLE_KEY(Key_F8)
    HANDLE_KEY(Key_F9)
    HANDLE_KEY(Key_F10)
    HANDLE_KEY(Key_F11)
    HANDLE_KEY(Key_F12)
    HANDLE_KEY(Key_F13)
    HANDLE_KEY(Key_F14)
    HANDLE_KEY(Key_F15)
    HANDLE_KEY(Key_F16)
    HANDLE_KEY(Key_F17)
    HANDLE_KEY(Key_F18)
    HANDLE_KEY(Key_F19)
    HANDLE_KEY(Key_F20)
    HANDLE_KEY(Key_F21)
    HANDLE_KEY(Key_F22)
    HANDLE_KEY(Key_F23)
    HANDLE_KEY(Key_F24)
    
    // F25 .. F35 only on X11
    HANDLE_KEY(Key_F25)
    HANDLE_KEY(Key_F26)
    HANDLE_KEY(Key_F27)
    HANDLE_KEY(Key_F28)
    HANDLE_KEY(Key_F29)
    HANDLE_KEY(Key_F30)
    HANDLE_KEY(Key_F31)
    HANDLE_KEY(Key_F32)
    HANDLE_KEY(Key_F33)
    HANDLE_KEY(Key_F34)
    HANDLE_KEY(Key_F35)
    
    // extra keys
    HANDLE_KEY(Key_Super_L)
    HANDLE_KEY(Key_Super_R)
    HANDLE_KEY(Key_Menu)
    HANDLE_KEY(Key_Hyper_L)
    HANDLE_KEY(Key_Hyper_R)
    HANDLE_KEY(Key_Help)
    HANDLE_KEY(Key_Space)

    // 7 bit printable ASCII
    HANDLE_KEY(Key_Exclam)
    HANDLE_KEY(Key_QuoteDbl)
    HANDLE_KEY(Key_NumberSign)
    HANDLE_KEY(Key_Dollar)
    HANDLE_KEY(Key_Percent)
    HANDLE_KEY(Key_Ampersand)
    HANDLE_KEY(Key_Apostrophe)
    HANDLE_KEY(Key_ParenLeft)
    HANDLE_KEY(Key_ParenRight)
    HANDLE_KEY(Key_Asterisk)
    HANDLE_KEY(Key_Plus)
    HANDLE_KEY(Key_Comma)
    HANDLE_KEY(Key_Minus)
    HANDLE_KEY(Key_Period)
    HANDLE_KEY(Key_Slash)
    HANDLE_KEY(Key_0)
    HANDLE_KEY(Key_1)
    HANDLE_KEY(Key_2)
    HANDLE_KEY(Key_3)
    HANDLE_KEY(Key_4)
    HANDLE_KEY(Key_5)
    HANDLE_KEY(Key_6)
    HANDLE_KEY(Key_7)
    HANDLE_KEY(Key_8)
    HANDLE_KEY(Key_9)
    HANDLE_KEY(Key_Colon)
    HANDLE_KEY(Key_Semicolon)
    HANDLE_KEY(Key_Less)
    HANDLE_KEY(Key_Equal)
    HANDLE_KEY(Key_Greater)
    HANDLE_KEY(Key_Question)
    HANDLE_KEY(Key_At)
    HANDLE_KEY(Key_A)
    HANDLE_KEY(Key_B)
    HANDLE_KEY(Key_C)
    HANDLE_KEY(Key_D)
    HANDLE_KEY(Key_E)
    HANDLE_KEY(Key_F)
    HANDLE_KEY(Key_G)
    HANDLE_KEY(Key_H)
    HANDLE_KEY(Key_I)
    HANDLE_KEY(Key_J)
    HANDLE_KEY(Key_K)
    HANDLE_KEY(Key_L)
    HANDLE_KEY(Key_M)
    HANDLE_KEY(Key_N)
    HANDLE_KEY(Key_O)
    HANDLE_KEY(Key_P)
    HANDLE_KEY(Key_Q)
    HANDLE_KEY(Key_R)
    HANDLE_KEY(Key_S)
    HANDLE_KEY(Key_T)
    HANDLE_KEY(Key_U)
    HANDLE_KEY(Key_V)
    HANDLE_KEY(Key_W)
    HANDLE_KEY(Key_X)
    HANDLE_KEY(Key_Y)
    HANDLE_KEY(Key_Z)
    HANDLE_KEY(Key_BracketLeft)
    HANDLE_KEY(Key_Backslash)
    HANDLE_KEY(Key_BracketRight)
    HANDLE_KEY(Key_AsciiCircum)
    HANDLE_KEY(Key_Underscore)
    HANDLE_KEY(Key_QuoteLeft)
    HANDLE_KEY(Key_BraceLeft)
    HANDLE_KEY(Key_Bar)
    HANDLE_KEY(Key_BraceRight)
    HANDLE_KEY(Key_AsciiTilde)

    // HANDLE_KEY(Latin)

    HANDLE_KEY(Key_nobreakspace)
    HANDLE_KEY(Key_exclamdown)
    HANDLE_KEY(Key_cent)
    HANDLE_KEY(Key_sterling)
    HANDLE_KEY(Key_currency)
    HANDLE_KEY(Key_yen)
    HANDLE_KEY(Key_brokenbar)
    HANDLE_KEY(Key_section)
    HANDLE_KEY(Key_diaeresis)
    HANDLE_KEY(Key_copyright)
    HANDLE_KEY(Key_ordfeminine)
    HANDLE_KEY(Key_guillemotleft)
    HANDLE_KEY(Key_notsign)
    HANDLE_KEY(Key_hyphen)
    HANDLE_KEY(Key_registered)
    HANDLE_KEY(Key_macron)
    HANDLE_KEY(Key_degree)
    HANDLE_KEY(Key_plusminus)
    HANDLE_KEY(Key_twosuperior)
    HANDLE_KEY(Key_threesuperior)
    HANDLE_KEY(Key_acute)
    HANDLE_KEY(Key_mu)
    HANDLE_KEY(Key_paragraph)
    HANDLE_KEY(Key_periodcentered)
    HANDLE_KEY(Key_cedilla)
    HANDLE_KEY(Key_onesuperior)
    HANDLE_KEY(Key_masculine)
    HANDLE_KEY(Key_guillemotright)
    HANDLE_KEY(Key_onequarter)
    HANDLE_KEY(Key_onehalf)
    HANDLE_KEY(Key_threequarters)
    HANDLE_KEY(Key_questiondown)
    HANDLE_KEY(Key_Agrave)
    HANDLE_KEY(Key_Aacute)
    HANDLE_KEY(Key_Acircumflex)
    HANDLE_KEY(Key_Atilde)
    HANDLE_KEY(Key_Adiaeresis)
    HANDLE_KEY(Key_Aring)
    HANDLE_KEY(Key_AE)
    HANDLE_KEY(Key_Ccedilla)
    HANDLE_KEY(Key_Egrave)
    HANDLE_KEY(Key_Eacute)
    HANDLE_KEY(Key_Ecircumflex)
    HANDLE_KEY(Key_Ediaeresis)
    HANDLE_KEY(Key_Igrave)
    HANDLE_KEY(Key_Iacute)
    HANDLE_KEY(Key_Icircumflex)
    HANDLE_KEY(Key_Idiaeresis)
    HANDLE_KEY(Key_ETH)
    HANDLE_KEY(Key_Ntilde)
    HANDLE_KEY(Key_Ograve)
    HANDLE_KEY(Key_Oacute)
    HANDLE_KEY(Key_Ocircumflex)
    HANDLE_KEY(Key_Otilde)
    HANDLE_KEY(Key_Odiaeresis)
    HANDLE_KEY(Key_multiply)
    HANDLE_KEY(Key_Ooblique)
    HANDLE_KEY(Key_Ugrave)
    HANDLE_KEY(Key_Uacute)
    HANDLE_KEY(Key_Ucircumflex)
    HANDLE_KEY(Key_Udiaeresis)
    HANDLE_KEY(Key_Yacute)
    HANDLE_KEY(Key_THORN)
    HANDLE_KEY(Key_ssharp)
    HANDLE_KEY(Key_agrave)
    HANDLE_KEY(Key_aacute)
    HANDLE_KEY(Key_acircumflex)
    HANDLE_KEY(Key_atilde)
    HANDLE_KEY(Key_adiaeresis)
    HANDLE_KEY(Key_aring)
    HANDLE_KEY(Key_ae)
    HANDLE_KEY(Key_ccedilla)
    HANDLE_KEY(Key_egrave)
    HANDLE_KEY(Key_eacute)
    HANDLE_KEY(Key_ecircumflex)
    HANDLE_KEY(Key_ediaeresis)
    HANDLE_KEY(Key_igrave)
    HANDLE_KEY(Key_iacute)
    HANDLE_KEY(Key_icircumflex)
    HANDLE_KEY(Key_idiaeresis)
    HANDLE_KEY(Key_eth)
    HANDLE_KEY(Key_ntilde)
    HANDLE_KEY(Key_ograve)
    HANDLE_KEY(Key_oacute)
    HANDLE_KEY(Key_ocircumflex)
    HANDLE_KEY(Key_otilde)
    HANDLE_KEY(Key_odiaeresis)
    HANDLE_KEY(Key_division)
    HANDLE_KEY(Key_oslash)
    HANDLE_KEY(Key_ugrave)
    HANDLE_KEY(Key_uacute)
    HANDLE_KEY(Key_ucircumflex)
    HANDLE_KEY(Key_udiaeresis)
    HANDLE_KEY(Key_yacute)
    HANDLE_KEY(Key_thorn)
    HANDLE_KEY(Key_ydiaeresis)

    HANDLE_KEY(Key_unknown)

    default: return "(unknown)";
  }
  
  #undef HANDLE_KEY
}


string toString(QKeyEvent const &k)
{
  return stringc << toString(k.state()) << "+" << toString((Qt::Key)(k.key()));
}


QString toQString(string const &s)
{
  return QString(s.c_str());
}


// EOF
