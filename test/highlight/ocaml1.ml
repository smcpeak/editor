(* ocaml1.ml *)
(* Testing the OCaml highlighter. *)

let x: int = 5

(* comment *)+1
(* 1 (* 2 *) 1 *)+1
(**)+1(***)+1(****)+1
(*) comment *)+1
(* (*) 2 *) 1 *)+1

(* ident *)
ident
_ident
id123_'546

Capitalized
lowercase

let house_number = 37
let million = 1_000_000
let copyright = 0x00A9
let counter64bit = ref 0L;;

let pi = 3.141_592_653_589_793_12
let small_negative = -1e-5
let machine_epsilon = 0x1p-52;;

let a = 'a'
let single_quote = '\''
let copyright = '\xA9';;

let greeting = "Hello, World!\n"
let superscript_plus = "\u{207A}";;

(* This text has some UTF-8 em dashes. *)
let longstr =
    "Call me Ishmael. Some years ago — never mind how long \
    precisely — having little or no money in my purse, and \
    nothing particular to interest me on shore, I thought I\
    \ would sail about a little and see the watery part of t\
    he world.";;

(* This does not work yet. *)
let quoted_greeting = {|"Hello, World!"|}
let nested = {ext|hello {|world|}|ext};;

(* Some operators. *)
$+ & * + - / = > @ ^ | ~+ ! ?+ % < #: !.
#* #+ <=> !!
(a b c)

type ('a, 'b) fnptr

(* Avoid these. *)
parser    value    $     $$    $:    <:    <<    >>    ??

(* Line directive. *)
#123 "somewhere"

(* Not line directive. *)
 #123 "somewhere"

(* EOF *)
