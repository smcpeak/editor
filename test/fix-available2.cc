// fix-available2.cc
// Code with an issue for which `clangd` offers a fix.

int main()
{
  // Fix: Add "#include <iostream>" near the top.
  //
  // This fix is not proposed right after `clangd` starts; it has to
  // index some other files first.
  std::cout << "hello\n";
  return 0;
}

// EOF
