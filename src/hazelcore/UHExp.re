open Sexplib.Std;

[@deriving sexp]
type operator = Operators_Exp.t;

[@deriving sexp]
type t = block
and block = list(line)
and line =
  | EmptyLine
  | CommentLine(string)
  | LetLine(UHPat.t, t)
  | ExpLine(opseq)
and opseq = OpSeq.t(operand, operator)
and operand =
  | EmptyHole(MetaVar.t)
  | InvalidText(MetaVar.t, string)
  | Var(ErrStatus.t, VarErrStatus.t, Var.t)
  | IntLit(ErrStatus.t, string)
  | FloatLit(ErrStatus.t, string)
  | BoolLit(ErrStatus.t, bool)
  | ListNil(ErrStatus.t)
  | Lam(ErrStatus.t, UHPat.t, t)
  | Inj(ErrStatus.t, InjSide.t, t)
  | Case(CaseErrStatus.t, t, rules)
  | Parenthesized(t)
  | ApPalette(ErrStatus.t, PaletteName.t, SerializedModel.t, splice_info)
and rules = list(rule)
and rule =
  | Rule(UHPat.t, t)
and splice_info = SpliceInfo.t(t)
and splice_map = SpliceInfo.splice_map(t);

[@deriving sexp]
type skel = OpSeq.skel(operator);
[@deriving sexp]
type seq = OpSeq.seq(operand, operator);

type affix = Seq.affix(operand, operator);

let letline = (p: UHPat.t, def: t): line => LetLine(p, def);

let var =
    (
      ~err: ErrStatus.t=NotInHole,
      ~var_err: VarErrStatus.t=NotInVarHole,
      x: Var.t,
    )
    : operand =>
  Var(err, var_err, x);

let intlit = (~err: ErrStatus.t=NotInHole, n: string): operand =>
  IntLit(err, n);

let floatlit = (~err: ErrStatus.t=NotInHole, f: string): operand =>
  FloatLit(err, f);

let boollit = (~err: ErrStatus.t=NotInHole, b: bool): operand =>
  BoolLit(err, b);

let lam = (~err: ErrStatus.t=NotInHole, p: UHPat.t, body: t): operand =>
  Lam(err, p, body);

let inj = (~err: ErrStatus.t=NotInHole, side: InjSide.t, body: t): operand =>
  Inj(err, side, body);

let case =
    (
      ~err: CaseErrStatus.t=StandardErrStatus(NotInHole),
      scrut: t,
      rules: rules,
    )
    : operand =>
  Case(err, scrut, rules);

let listnil = (~err: ErrStatus.t=NotInHole, ()): operand => ListNil(err);

module Line = {
  let prune_empty_hole = (line: line): line =>
    switch (line) {
    | ExpLine(OpSeq(_, S(EmptyHole(_), E))) => EmptyLine
    | CommentLine(_)
    | ExpLine(_)
    | EmptyLine
    | LetLine(_) => line
    };

  let get_opseq =
    fun
    | EmptyLine
    | CommentLine(_)
    | LetLine(_) => None
    | ExpLine(opseq) => Some(opseq);
  let force_get_opseq = line =>
    line
    |> get_opseq
    |> OptUtil.get(_ => failwith("force_get_opseq: expected ExpLine"));
};

module Lines = {
  let prune_empty_holes = (lines: list(line)): list(line) =>
    lines |> List.map(Line.prune_empty_hole);
};

module Block = {
  let wrap' = (opseq: opseq): block => [ExpLine(opseq)];
  let wrap = (operand: operand): block => wrap'(OpSeq.wrap(operand));

  let num_lines: block => int = List.length;

  let prune_empty_hole_lines = (block: block): block =>
    switch (block |> ListUtil.split_last_opt) {
    | None => block
    | Some((leading, last)) => (leading |> Lines.prune_empty_holes) @ [last]
    };

  let split_conclusion = (block: block): option((list(line), opseq)) =>
    switch (block |> ListUtil.split_last_opt) {
    | None => None
    | Some((leading, last)) =>
      switch (last |> Line.get_opseq) {
      | None => None
      | Some(opseq) => Some((leading, opseq))
      }
    };
  let force_split_conclusion = (block: block): (list(line), opseq) =>
    switch (block |> split_conclusion) {
    | None => failwith("force_split_conclusion: unconcluded block")
    | Some((leading, conclusion)) => (leading, conclusion)
    };

  let join_conclusion = (leading: list(line), conclusion: opseq): block =>
    leading @ [ExpLine(conclusion)];
};

let rec get_tuple_elements: skel => list(skel) =
  fun
  | BinOp(_, Comma, skel1, skel2) =>
    get_tuple_elements(skel1) @ get_tuple_elements(skel2)
  | skel => [skel];

let rec mk_tuple = (~err: ErrStatus.t=NotInHole, elements: list(skel)): skel =>
  switch (elements) {
  | [] => failwith("mk_tuple: expected at least 1 element")
  | [skel] => skel
  | [skel, ...skels] => BinOp(err, Comma, skel, mk_tuple(skels))
  };

let new_InvalidText =
    (u_gen: MetaVarGen.t, t: string): (operand, MetaVarGen.t) => {
  let (u, u_gen) = MetaVarGen.next(u_gen);
  (InvalidText(u, t), u_gen);
};

/* helper function for constructing a new empty hole */
let new_EmptyHole = (u_gen: MetaVarGen.t): (operand, MetaVarGen.t) => {
  let (u, u_gen) = u_gen |> MetaVarGen.next;
  (EmptyHole(u), u_gen);
};

let is_EmptyHole =
  fun
  | EmptyHole(_) => true
  | _ => false;

let empty_rule = (u_gen: MetaVarGen.t): (rule, MetaVarGen.t) => {
  let (p, u_gen) = UHPat.new_EmptyHole(u_gen);
  let (e, u_gen) = new_EmptyHole(u_gen);
  let rule = Rule(OpSeq.wrap(p), Block.wrap(e));
  (rule, u_gen);
};

let rec get_err_status = (e: t): ErrStatus.t => get_err_status_block(e)
and get_err_status_block = block => {
  let (_, conclusion) = block |> Block.force_split_conclusion;
  conclusion |> get_err_status_opseq;
}
and get_err_status_opseq = opseq =>
  OpSeq.get_err_status(~get_err_status_operand, opseq)
and get_err_status_operand =
  fun
  | EmptyHole(_) => NotInHole
  | InvalidText(_, _) => NotInHole
  | Var(err, _, _)
  | IntLit(err, _)
  | FloatLit(err, _)
  | BoolLit(err, _)
  | ListNil(err)
  | Lam(err, _, _)
  | Inj(err, _, _)
  | Case(StandardErrStatus(err), _, _)
  | ApPalette(err, _, _, _) => err
  | Case(InconsistentBranches(_), _, _) => NotInHole
  | Parenthesized(e) => get_err_status(e);

/* put e in the specified hole */
let rec set_err_status = (err: ErrStatus.t, e: t): t =>
  e |> set_err_status_block(err)
and set_err_status_block = (err: ErrStatus.t, block: block): block => {
  let (leading, conclusion) = block |> Block.force_split_conclusion;
  Block.join_conclusion(leading, conclusion |> set_err_status_opseq(err));
}
and set_err_status_opseq = (err, opseq) =>
  OpSeq.set_err_status(~set_err_status_operand, err, opseq)
and set_err_status_operand = (err, operand) =>
  switch (operand) {
  | EmptyHole(_) => operand
  | InvalidText(_, _) => operand
  | Var(_, var_err, x) => Var(err, var_err, x)
  | IntLit(_, n) => IntLit(err, n)
  | FloatLit(_, f) => FloatLit(err, f)
  | BoolLit(_, b) => BoolLit(err, b)
  | ListNil(_) => ListNil(err)
  | Lam(_, p, def) => Lam(err, p, def)
  | Inj(_, inj_side, body) => Inj(err, inj_side, body)
  | Case(_, scrut, rules) => Case(StandardErrStatus(err), scrut, rules)
  | ApPalette(_, name, model, si) => ApPalette(err, name, model, si)
  | Parenthesized(body) => Parenthesized(body |> set_err_status(err))
  };

let is_inconsistent = operand =>
  switch (operand |> get_err_status_operand) {
  | InHole(TypeInconsistent, _) => true
  | _ => false
  };

/* put e in a new hole, if it is not already in a hole */
let rec mk_inconsistent = (u_gen: MetaVarGen.t, e: t): (t, MetaVarGen.t) =>
  mk_inconsistent_block(u_gen, e)
and mk_inconsistent_block =
    (u_gen: MetaVarGen.t, block: block): (block, MetaVarGen.t) => {
  let (leading, conclusion) = block |> Block.force_split_conclusion;
  let (conclusion, u_gen) = conclusion |> mk_inconsistent_opseq(u_gen);
  (Block.join_conclusion(leading, conclusion), u_gen);
}
and mk_inconsistent_opseq = (u_gen, opseq) =>
  OpSeq.mk_inconsistent(~mk_inconsistent_operand, u_gen, opseq)
and mk_inconsistent_operand = (u_gen, operand) =>
  switch (operand) {
  /* already in hole */
  | EmptyHole(_)
  | InvalidText(_, _)
  | Var(InHole(TypeInconsistent, _), _, _)
  | IntLit(InHole(TypeInconsistent, _), _)
  | FloatLit(InHole(TypeInconsistent, _), _)
  | BoolLit(InHole(TypeInconsistent, _), _)
  | ListNil(InHole(TypeInconsistent, _))
  | Lam(InHole(TypeInconsistent, _), _, _)
  | Inj(InHole(TypeInconsistent, _), _, _)
  | Case(StandardErrStatus(InHole(TypeInconsistent, _)), _, _)
  | ApPalette(InHole(TypeInconsistent, _), _, _, _) => (operand, u_gen)
  /* not in hole */
  | Var(NotInHole | InHole(WrongLength, _), _, _)
  | IntLit(NotInHole | InHole(WrongLength, _), _)
  | FloatLit(NotInHole | InHole(WrongLength, _), _)
  | BoolLit(NotInHole | InHole(WrongLength, _), _)
  | ListNil(NotInHole | InHole(WrongLength, _))
  | Lam(NotInHole | InHole(WrongLength, _), _, _)
  | Inj(NotInHole | InHole(WrongLength, _), _, _)
  | Case(
      StandardErrStatus(NotInHole | InHole(WrongLength, _)) |
      InconsistentBranches(_, _),
      _,
      _,
    )
  | ApPalette(NotInHole | InHole(WrongLength, _), _, _, _) =>
    let (u, u_gen) = u_gen |> MetaVarGen.next;
    let operand =
      operand |> set_err_status_operand(InHole(TypeInconsistent, u));
    (operand, u_gen);
  /* err in constructor args */
  | Parenthesized(body) =>
    let (body, u_gen) = body |> mk_inconsistent(u_gen);
    (Parenthesized(body), u_gen);
  };

let text_operand =
    (u_gen: MetaVarGen.t, shape: TextShape.t): (operand, MetaVarGen.t) =>
  switch (shape) {
  | Underscore => (var("_"), u_gen)
  | IntLit(n) => (intlit(n), u_gen)
  | FloatLit(f) => (floatlit(f), u_gen)
  | BoolLit(b) => (boollit(b), u_gen)
  | Var(x) => (var(x), u_gen)
  | ExpandingKeyword(kw) =>
    let (u, u_gen) = u_gen |> MetaVarGen.next;
    (
      var(~var_err=InVarHole(Free, u), kw |> ExpandingKeyword.to_string),
      u_gen,
    );
  | InvalidTextShape(t) => new_InvalidText(u_gen, t)
  };

let associate =
  Skel.mk(Operators_Exp.precedence, Operators_Exp.associativity);

let mk_OpSeq = OpSeq.mk(~associate);

let rec is_complete_line = (l: line): bool => {
  switch (l) {
  | EmptyLine
  | CommentLine(_) => true
  | LetLine(pat, body) => UHPat.is_complete(pat) && is_complete(body)
  | ExpLine(body) => OpSeq.is_complete(is_complete_operand, body)
  };
}
and is_complete_block = (b: block): bool => {
  b |> List.for_all(l => is_complete_line(l));
}
and is_complete_rule = (rule: rule): bool => {
  switch (rule) {
  | Rule(pat, body) => UHPat.is_complete(pat) && is_complete(body)
  };
}
and is_complete_rules = (rules: rules): bool => {
  rules |> List.for_all(l => is_complete_rule(l));
}
and is_complete_operand = (operand: 'operand): bool => {
  switch (operand) {
  | EmptyHole(_) => false
  | InvalidText(_, _) => false
  | Var(InHole(_), _, _) => false
  | Var(NotInHole, InVarHole(_), _) => false
  | Var(NotInHole, NotInVarHole, _) => true
  | IntLit(InHole(_), _) => false
  | IntLit(NotInHole, _) => true
  | FloatLit(InHole(_), _) => false
  | FloatLit(NotInHole, _) => true
  | BoolLit(InHole(_), _) => false
  | BoolLit(NotInHole, _) => true
  | ListNil(InHole(_)) => false
  | ListNil(NotInHole) => true
  | Lam(InHole(_), _, _) => false
  | Lam(NotInHole, pat, body) => UHPat.is_complete(pat) && is_complete(body)
  | Inj(InHole(_), _, _) => false
  | Inj(NotInHole, _, body) => is_complete(body)
  | Case(StandardErrStatus(InHole(_)) | InconsistentBranches(_), _, _) =>
    false
  | Case(StandardErrStatus(NotInHole), body, rules) =>
    is_complete(body) && is_complete_rules(rules)
  | Parenthesized(body) => is_complete(body)
  | ApPalette(InHole(_), _, _, _) => false
  | ApPalette(NotInHole, _, _, _) => failwith("unimplemented")
  };
}
and is_complete = (exp: t): bool => {
  is_complete_block(exp);
};

let fill_hole = (u: MetaVar.t, filler: t, e: t) => {
  open OptUtil.Syntax;
  let rec go_block = (block: block): option(block) => {
    let (filled, block) =
      List.fold_right(
        (line, (filled, lines)) =>
          filled
            ? (true, [line, ...lines])
            : (
              switch (go_line(line)) {
              | None => (false, [line, ...lines])
              | Some(filled_line) => (true, [filled_line, ...lines])
              }
            ),
        block,
        (false, []),
      );
    filled ? Some(block) : None;
  }
  and go_line = line =>
    switch (line) {
    | EmptyLine
    | CommentLine(_) => None
    | LetLine(p, def) =>
      let+ filled_def = go_block(def);
      LetLine(p, filled_def);
    | ExpLine(OpSeq(_, seq)) =>
      let+ filled_seq = go_seq(seq);
      ExpLine(mk_OpSeq(filled_seq));
    }
  and go_seq: seq => option(seq) =
    fun
    | S(operand, affix) =>
      switch (go_operand(operand)) {
      | None =>
        let+ filled_affix = go_affix(affix);
        Seq.S(operand, filled_affix);
      | Some(filled_operand) => Some(S(filled_operand, affix))
      }
  and go_affix: affix => option(affix) =
    fun
    | E => None
    | A(op, seq) => {
        let+ filled_seq = go_seq(seq);
        Seq.A(op, filled_seq);
      }
  and go_operand = (operand: operand): option(operand) =>
    switch (operand) {
    | InvalidText(_)
    | Var(_)
    | IntLit(_)
    | FloatLit(_)
    | BoolLit(_)
    | ListNil(_)
    | ApPalette(_) => None
    | EmptyHole(u') =>
      if (u == u') {
        switch (filler) {
        | [ExpLine(OpSeq(_, S(operand, E)))] => Some(operand)
        | _ => Some(Parenthesized(filler))
        };
      } else {
        None;
      }
    | Parenthesized(body) =>
      let+ filled_body = go_block(body);
      Parenthesized(filled_body);
    | Inj(err, side, body) =>
      let+ filled_body = go_block(body);
      Inj(err, side, filled_body);
    | Lam(err, p, def) =>
      let+ filled_def = go_block(def);
      Lam(err, p, filled_def);
    | Case(err, scrut, rules) =>
      switch (go_block(scrut)) {
      | Some(filled_scrut) => Some(Case(err, filled_scrut, rules))
      | None =>
        let (filled, rules) =
          List.fold_right(
            (rule, (filled, rules)) =>
              filled
                ? (true, [rule, ...rules])
                : (
                  switch (go_rule(rule)) {
                  | None => (false, [rule, ...rules])
                  | Some(filled_rule) => (true, [filled_rule, ...rules])
                  }
                ),
            rules,
            (false, []),
          );
        filled ? Some(Case(err, scrut, rules)) : None;
      }
    }
  and go_rule = (Rule(p, clause): rule) => {
    let+ filled_clause = go_block(clause);
    Rule(p, filled_clause);
  };
  switch (go_block(e)) {
  | None => e
  | Some(filled) => filled
  };
};

let is_atomic_operand: operand => bool =
  fun
  | EmptyHole(_)
  | InvalidText(_)
  | Var(_)
  | IntLit(_)
  | FloatLit(_)
  | BoolLit(_)
  | ListNil(_) => true
  | Lam(_)
  | Inj(_)
  | Case(_)
  | Parenthesized(_)
  | ApPalette(_) => false;

/* NOTE: Should be replaced when parser is ready */
let operand_of_string = (text: string): operand =>
  switch (TextShape.of_text(text)) {
  | IntLit(s) => intlit(s)
  | FloatLit(s) => floatlit(s)
  | BoolLit(s) => boollit(s)
  | ExpandingKeyword(Let) => var("let")
  | ExpandingKeyword(Case) => var("case")
  | Underscore => var("_")
  | Var(s) => var(s)
  | InvalidTextShape(s) => InvalidText(0, s)
  };

/* NOTE: Should be replaced with proper to_string when parser is ready
   Right now this special-cases case and binary apps for assistant
   suggestion text-matching purposes. */
let rec string_of_operand = (operand: operand): string => {
  switch (operand) {
  | InvalidText(_, s)
  | Var(_, _, s)
  | IntLit(_, s)
  | FloatLit(_, s) => s
  | BoolLit(_, b) => string_of_bool(b)
  | Inj(_, side, _) => "inj" ++ InjSide.to_string(side) ++ ""
  | Lam(_) => "\\"
  | Case(_, [ExpLine(OpSeq(_, S(operandA, _)))], _) =>
    "case " ++ string_of_operand(operandA)
  | Case(_, _, _) => "case"
  | Parenthesized([
      ExpLine(OpSeq(_, S(operandA, A(Space, S(operandB, _))))),
    ]) =>
    string_of_operand(operandA) ++ " " ++ string_of_operand(operandB)
  | Parenthesized([ExpLine(OpSeq(_, S(operandA, _)))]) =>
    string_of_operand(operandA)
  | ListNil(_) => "[]"
  | Parenthesized(_)
  | EmptyHole(_)
  | ApPalette(_) => ""
  };
};

let operators_of_ty =
    (l: HTyp.t, r: HTyp.t, out: HTyp.t): list(Operators_Exp.t) =>
  List.concat([
    HTyp.consistent_all([l, r, out, HTyp.Bool])
      ? Operators_Exp.[And, Or] : [],
    HTyp.consistent_all([l, r, out, HTyp.Int])
      ? Operators_Exp.[Plus, Minus, Times, Divide] : [],
    HTyp.consistent_all([l, r, out, HTyp.Float])
      ? Operators_Exp.[FPlus, FMinus, FTimes, FDivide] : [],
    HTyp.consistent_all([l, r, HTyp.Int]) && HTyp.consistent(out, HTyp.Bool)
      ? Operators_Exp.[LessThan, GreaterThan, Equals] : [],
    HTyp.consistent_all([l, r, HTyp.Float])
    && HTyp.consistent(out, HTyp.Bool)
      ? Operators_Exp.[FLessThan, FGreaterThan, FEquals] : [],
  ]);
