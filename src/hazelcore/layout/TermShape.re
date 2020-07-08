open Sexplib.Std;

[@deriving sexp]
type t =
  | Rule
  | Invalid
  | Case({err: CaseErrStatus.t})
  | Var({
      err: ErrStatus.t,
      verr: VarErrStatus.t,
      show_use: bool,
    })
  | Operand({err: ErrStatus.t})
  | FreeLivelit
  | ApLivelit
  | LivelitExpression({hd_index: int})
  | BinOp({
      op_index: int,
      err: ErrStatus.t,
    })
  | NTuple({
      comma_indices: list(int),
      err: ErrStatus.t,
    })
  | SubBlock({
      // TODO fix this hack when we have proper tiles
      hd: UHExp.line,
      hd_index: int,
    });

let mk_Var =
    (
      ~err: ErrStatus.t=NotInHole,
      ~verr: VarErrStatus.t=NotInVarHole,
      ~show_use=false,
      (),
    ) =>
  Var({err, verr, show_use});

let mk_Operand = (~err: ErrStatus.t=NotInHole, ()) => Operand({err: err});
