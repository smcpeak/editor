// testgap.cc
// test the gap.h module

#include "gap.h"     // module to test
#include "test.h"    // ARGS_MAIN

#include <stdio.h>   // printf
#include <stdlib.h>  // rand, srand
#include <time.h>    // time


// reference implementation of a sequence of ints
class Sequence {
private:
  int *arr;       // all elements in sequence, contiguously
  int len;        // # of elements in sequence, and length of 'arr'
  
private:
  void bc(int elt) const { xassert(0 <= elt && elt < len); }

public:
  Sequence() : arr(NULL), len(0) {}
  ~Sequence() { if (arr) delete[] arr; }

  int length() const { return len; }

  int get(int elt) const              { bc(elt); return arr[elt]; }
  void set(int elt, int value) const  { bc(elt); arr[elt] = value; }

  void insert(int elt, int value);
  void remove(int elt);

  void insertMany(int elt, int const *src, int srcLen) {
    while (srcLen--) {
      insert(elt++, *src++);
    }
  }

  void removeMany(int elt, int numElts) {
    while (numElts--) {
      remove(elt);
    }
  }

  void clear() {
    removeMany(0, length());
  }

  void fillFromArray(int const *src, int srcLen, int elt=0, int gapSize=0) {
    clear();
    insertMany(elt, src, srcLen);
  }

  void writeIntoArray(int *dest) const {
    memcpy(dest, arr, len * sizeof(int));
  }
};


void Sequence::insert(int elt, int value)
{
  xassert(0 <= elt && elt <= len);

  // make a new array every time
  int *newArr = new int[len+1];

  // fill it
  memcpy(newArr, arr, elt * sizeof(int));
  newArr[elt] = value;
  memcpy(newArr+elt+1, arr+elt, (len-elt) * sizeof(int));

  if (arr) { delete[] arr; }
  arr = newArr;
  len++;
}


void Sequence::remove(int elt)
{
  xassert(0 <= elt && elt < len);

  // make a new array every time
  int *newArr = new int[len-1];

  // fill it
  memcpy(newArr, arr, elt * sizeof(int));
  memcpy(newArr+elt, arr+elt+1, (len-elt-1) * sizeof(int));

  if (arr) { delete[] arr; }
  arr = newArr;
  len--;
}


// check that the two sequences match
void checkEqual(GapArray<int> const &seq1, Sequence const &seq2)
{
  // test length()
  xassert(seq1.length() == seq2.length());
  int len = seq1.length();
                  
  // test get()
  for (int i=0; i<len; i++) {
    xassert(seq1.get(i) == seq2.get(i));
  }

  // test writeIntoArray()
  int *temp1 = new int[len+1];
  int *temp2 = new int[len+1];
  temp1[len] = 0xABCDEF;   // canary
  temp2[len] = 0xABCDEF;

  seq1.writeIntoArray(temp1);
  seq2.writeIntoArray(temp2);

  xassert(temp1[len] == 0xABCDEF);
  xassert(temp2[len] == 0xABCDEF);

  xassert(0==memcpy(temp1, temp2, len * sizeof(int)));
  
  delete[] temp1;
  delete[] temp2;
}


int ctSet=0, ctInsert=0, ctInsertMany=0, ctRemove=0, ctRemoveMany=0, ctClear=0,
    ctFillFromArray=0;


// apply a random operation to both sequences
void mutate(GapArray<int> &seq1, Sequence &seq2)
{
  int choice = rand() % 100;

  // use set()
  if (choice < 20) {
    ctSet++;
    int elt = rand() % seq1.length();
    int val = rand();
    seq1.set(elt, val);
    seq2.set(elt, val);
  }

  // use insert()
  else if (choice < 40) {
    ctInsert++;
    int elt = rand() % (seq1.length() + 1);
    int val = rand();
    seq1.insert(elt, val);
    seq2.insert(elt, val);
  }

  // use insertMany()
  else if (choice < 60) {
    ctInsertMany++;
    int elt = rand() % (seq1.length() + 1);
    int sz = rand() % 20;
    int *temp = new int[sz];
    for (int i=0; i<sz; i++) {
      temp[i] = rand();
    }
    seq1.insertMany(elt, temp, sz);
    seq2.insertMany(elt, temp, sz);
  }

  // use remove()
  else if (choice < 80) {
    ctRemove++;
    int elt = rand() % seq1.length();
    seq1.remove(elt);
    seq2.remove(elt);
  }

  // use removeMany()
  else if (choice < 98) {
    ctRemoveMany++;
    int sz = rand() % 20;     // # to remove
    int elt = rand() % (seq1.length()+1 - sz);
    seq1.removeMany(elt, sz);
    seq2.removeMany(elt, sz);
  }

  // use fillFromArray
  else if (choice < 99) {
    ctFillFromArray++;
    int sz = rand() % 50;
    int gapElt = rand() % (sz+1);
    int gapSize = rand() % 20;
    int *temp = new int[sz];
    for (int i=0; i<sz; i++) {
      temp[i] = rand();
    }
    seq1.fillFromArray(temp, sz, gapElt, gapSize);
    seq2.fillFromArray(temp, sz, gapElt, gapSize);
  }

  // use clear()
  else {
    ctClear++;
    seq1.clear();
    seq2.clear();
  }
}


void entry(int argc, char *argv[])
{
  //srand(time());

  int iters = 100;
  if (argc >= 2) {
    iters = atoi(argv[1]);
  }
  printf("iters: %d\n", iters);

  GapArray<int> gap;
  Sequence seq;
  checkEqual(gap, seq);

  for (int i=0; i<iters; i++) {
    mutate(gap, seq);
    checkEqual(gap, seq);
  }

  printf("ok!\n");
}


ARGS_MAIN
