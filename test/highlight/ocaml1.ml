(* ocaml1.ml *)
(* Testing the OCaml highlighter. *)

let x: int = 5

(* comment *)+1
(* 1 (* 2 *) 1 *)+1
(**)+1(***)+1(****)+1
(*) comment *)+1
(* (*) 2 *) 1 *)+1

(* EOF *)
