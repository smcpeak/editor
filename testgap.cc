// testgap.cc
// test the gap.h module

#include "gap.h"     // module to test
#include "test.h"    // ARGS_MAIN
#include "ckheap.h"  // malloc_stats

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
    insertMany(0 /*elt*/, src, srcLen);
  }

  void writeIntoArray(int *dest, int destLen, int elt=0) const {
    xassert(destLen >= 0);
    xassert(0 <= elt && elt+destLen <= len);
    memcpy(dest, arr+elt, destLen * sizeof(int));
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

              
template <class Seq>
void printSeq(char const *prefix, Seq &seq)
{                                 
  printf("%s:", prefix);
  int len = seq.length();
  for (int i=0; i<len; i++) {
    int v = seq.get(i);
    printf(" %d", v);
  }                  
  printf("\n");
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
  int const CANARY = 0xABCDEF;
  temp1[len] = CANARY;
  temp2[len] = CANARY;

  seq1.writeIntoArray(temp1, len);
  seq2.writeIntoArray(temp2, len);

  xassert(temp1[len] == CANARY);
  xassert(temp2[len] == CANARY);

  xassert(0==memcmp(temp1, temp2, len * sizeof(int)));

  // write selected subsequences
  for (int elt=0; elt < len; elt += 10) {
    int amt = min(10, len-elt);

    // write from seq1, and verify it
    seq1.writeIntoArray(temp1+elt, amt, elt);
    xassert(temp1[len] == CANARY);
    xassert(0==memcmp(temp1+elt, temp2+elt, amt*sizeof(int)));

    // write from seq2, and verify it
    seq2.writeIntoArray(temp2+elt, amt, elt);
    xassert(temp2[len] == CANARY);
    xassert(0==memcmp(temp1+elt, temp2+elt, amt*sizeof(int)));
  }

  xassert(temp1[len] == CANARY);
  xassert(temp2[len] == CANARY);

  delete[] temp1;
  delete[] temp2;
}


int ctSet=0, ctInsert=0, ctInsertMany=0, ctRemove=0, ctRemoveMany=0, ctClear=0,
    ctFillFromArray=0;


int randValue()
{
  return rand() % 100;
}


// apply a random operation to both sequences
void mutate(GapArray<int> &seq1, Sequence &seq2)
{
  int choice = rand() % 100;

  // use set()
  if (choice < 20 && seq1.length()) {
    ctSet++;
    int elt = rand() % seq1.length();
    int val = randValue();
    seq1.set(elt, val);
    seq2.set(elt, val);
  }

  // use insert()
  else if (choice < 40) {
    ctInsert++;
    int elt = rand() % (seq1.length() + 1);
    int val = randValue();
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
      temp[i] = randValue();
    }
    seq1.insertMany(elt, temp, sz);
    seq2.insertMany(elt, temp, sz);
    delete[] temp;
  }

  // use remove()
  else if (choice < 80) {
    ctRemove++;
    int len = seq1.length();
    if (len) {
      int elt = rand() % len;
      seq1.remove(elt);
      seq2.remove(elt);
    }
  }

  // use removeMany()
  else if (choice < 98) {
    ctRemoveMany++;
    int len = seq1.length();
    int sz = rand() % (min(20, len+1));     // # to remove
    int elt = rand() % (len+1 - sz);
    seq1.removeMany(elt, sz);
    seq2.removeMany(elt, sz);
  }

  // use fillFromArray()
  else if (choice < 99) {
    ctFillFromArray++;
    int sz = rand() % 50;
    int gapElt = rand() % (sz+1);
    int gapSize = rand() % 20;
    int *temp = new int[sz];
    for (int i=0; i<sz; i++) {
      temp[i] = randValue();
    }
    seq1.fillFromArray(temp, sz, gapElt, gapSize);
    seq2.fillFromArray(temp, sz, gapElt, gapSize);
    delete[] temp;
  }

  // use clear()
  else {
    ctClear++;
    seq1.clear();
    seq2.clear();
  }
}


int const PRINT = 0;

void entry(int argc, char *argv[])
{
  //srand(time());

  malloc_stats();

  {
    int iters = 100;
    if (argc >= 2) {
      iters = atoi(argv[1]);
    }
    printf("iters: %d\n", iters);

    GapArray<int> gap;
    Sequence seq;

    if (PRINT) {
      printSeq("gap", gap);
      printSeq("seq", seq);
    }
    checkEqual(gap, seq);

    for (int i=0; i<iters; i++) {
      mutate(gap, seq);
      checkHeap();

      if (PRINT) {
        printSeq("gap", gap);
        printSeq("seq", seq);
      }
      checkEqual(gap, seq);
    }

    printf("ok!\n");
    printf("ctSet=%d ctInsert=%d ctInsertMany=%d ctRemove=%d\n"
           "ctRemoveMany=%d ctClear=%d ctFillFromArray=%d\n",
           ctSet, ctInsert, ctInsertMany, ctRemove,
           ctRemoveMany, ctClear, ctFillFromArray);
    printf("total: %d\n",
           ctSet + ctInsert + ctInsertMany + ctRemove +
           ctRemoveMany + ctClear + ctFillFromArray);
  }

  malloc_stats();
}


ARGS_MAIN
