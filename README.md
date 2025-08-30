## Scott's Editor

This repo contains a text editor written in Qt.  It is meant primarily
for editing software source code.

Some notable features:

* All features are meant to be primarily used via the keyboard, although
  basic mouse support (click to move, click+drag to select, menuing)
  exists.

* It allows the cursor to move anywhere in the infinite quarter-plane on
  which the text is laid out, rather than being confined to where the
  content currently has characters.  Typing beyond the end of line or
  file automatically fills the with whitespace as needed.

* It can edit files on remote machines while the editor runs locally,
  by making use of a server process that is started and communicated
  with over SSH.  (This requires that passwordless SSH be already set
  up.)

* Has an efficient search and replace interface that highlights all
  matches, allows easy navigation among them, replacing individual
  matches as desired.

* Has syntax highlighting for C/C++, Diff, Make, OCaml, and Python.
  Also has a mode to treat any line starting with "#" as a comment.
  New highlighting modes can be added reasonably easily by writing a
  flex-based lexer (actually
  [smflex](https://github.com/smcpeak/smflex), my lightly modified
  version of flex).

* Whitespace characters are shown in a color close to the background
  color that makes them visible but unobtrusive.

* A right margin (by default at 72 colums) is also unobtrusively shown
  as a guide to line length.

* Undo/redo.

* Basic macro support (work in progress).

* Indentation is semi-automatic; pressing enter moves the line to same
  column as the first character of the nearest preceding, non-blank
  line.  Contiguous blocks of lines can be easily indented or
  un-indented by selecting them and using Tab or Shift-Tab.

* The file browser (open file dialog) has basic tab completion, making
  opening files entirely with keyboard controls reasonably efficient.

* Switching between files is fairly efficient: Press Ctrl+O to get a
  dialog showing all open files, then (if desired) press F to highlight
  the search box and type a substring to filter.  Arrow to the desired
  file and press Enter.

* Can have any number of open windows showing the same underlying set of
  editors, with contents synchronized.  Each window remembers its own
  cursor position so you can edit different parts of the same file in
  different windows.

* The design is intended to support edititing large files efficiently,
  but some features (search and replace espcially) need further
  optimization.

* Can run an arbitrary command in the directory of an open file and see
  the output in an editor window.  (This is how build integration
  works, essentially.)

* Can pass any text selection (or nothing) through a command line and
  have the selection replaced by the command's output.

* Due to running on top of Qt, it is portable to Windows, Linux, and
  Mac, although I've never built it for Mac.

* C++ Language Server Protocol support with `clangd`.

Major limitations:

* No support for UTF-8 (right now it treats all text as Latin-1).

* No plugin interface.

* No support for debugger interaction.

* No support for calling out to AI tools.

These are all things I hope to address.

Medium (IMO) limitations:

* No multi-file search and replace.

* No fully automatic syntax-aware indentation.

* No full-featured revision control integration.  However, the "run
  command" interface allows things like "git blame", "git diff", etc.,
  to work adequately for my purposes.

* No multi-line cursor nor rectangular selections.

* No command to comment or un-comment a range of lines.

* The file brower (open file dialog) could be more full-featured.  This
  might include extending the main editor window to have file browsing
  capabilities the way emacs does.

I'm open to considering adding these, but they aren't a high priority
for me currently.  In part, that is because the "apply command" feature,
combined with an external script, can do a lot of what might be built in
to another editor.

## History

* Back in the day (late 1980s, early 1990s), I used Borland
  [Turbo C++](https://en.wikipedia.org/wiki/Turbo_C%2B%2B), and very much
  liked its editor.  My editor's key bindings and color choices are
  heavily influenced by those of Turbo C++.

* After moving to primarily working in a Linux environment in the late
  1990s, I spent years trying to get
  [emacs](https://en.wikipedia.org/wiki/Emacs) to work satisfactorily,
  including making it imitate Turbo C++ in a number of respects, but it
  was always an uphill fight.  A major issue was (and still is, I think)
  the particularly inefficient way emacs stores text files in memory,
  leading to very slow operation on large files.

* In the early 2000s, I started writing this editor, and got it to a
  somewhat usable state, but still with a lot of missing features.

* So for most of the 2000s and 2010s, I kept using emacs despite its
  issues.  I occasionally dabbled with this editor, but didn't seriously
  use it.

* In the late 2010s, I started using
  [Visual Studio Code](https://en.wikipedia.org/wiki/Visual_Studio_Code),
  which was much better than emacs.  But its
  [poor security situation](https://stackoverflow.com/questions/57874263/how-to-disable-internet-access-for-a-particular-vscode-extension-you-dont-trust/57877046)
  was a major source of concern, and it has some other usability issues
  that weren't easily resolved with plugins.

* So, in mid-2018 I resumed work on this editor, and over the course of
  a year or so finished the critical mass of features needed to use it
  as my primary editor, and have been using it as such since then.

## License

The license is the 2-clause BSD license: [license.txt](license.txt).
