open SemanticsCommon;
open HazelUtil;

[@deriving show({with_path: false})]
type cursor_side = SemanticsCommon.cursor_side;

type opseq_surround = OperatorSeq.opseq_surround(UHExp.t, UHExp.op);
type opseq_prefix = OperatorSeq.opseq_prefix(UHExp.t, UHExp.op);
type opseq_suffix = OperatorSeq.opseq_suffix(UHExp.t, UHExp.op);

type zblock =
  | CursorB(cursor_side, UHExp.block)
  | DeeperB(err_status, zblock')
and zblock' =
  | BlockZL(zlines, UHExp.t)
  | BlockZE(UHExp.lines, t)
and zlines = ZList.t(zline, UHExp.line)
and zline =
  | CursorL(cursor_side, UHExp.line)
  | DeeperL(zline')
and zline' =
  | ExpLineZ(t)
  | LetLineZP(ZPat.t, option(UHTyp.t), UHExp.block)
  | LetLineZA(UHPat.t, ZTyp.t, UHExp.block)
  | LetLineZE(UHPat.t, option(UHTyp.t), zblock)
and t =
  | CursorE(cursor_side, UHExp.t)
  | ParenthesizedZ(zblock)
  | OpSeqZ(UHExp.skel_t, t, opseq_surround)
  | DeeperE(err_status, t')
/* | CursorPalette : PaletteName.t -> PaletteSerializedModel.t -> hole_ref -> t -> t */
and t' =
  | LamZP(ZPat.t, option(UHTyp.t), UHExp.block)
  | LamZA(UHPat.t, ZTyp.t, UHExp.block)
  | LamZE(UHPat.t, option(UHTyp.t), zblock)
  | InjZ(inj_side, zblock)
  | CaseZE(zblock, list(UHExp.rule), option(UHTyp.t))
  | CaseZR(UHExp.block, zrules, option(UHTyp.t))
  | CaseZA(UHExp.block, list(UHExp.rule), ZTyp.t)
  | ApPaletteZ(
      PaletteName.t,
      SerializedModel.t,
      ZSpliceInfo.t(UHExp.block, zblock),
    )
and zrules = ZList.t(zrule, UHExp.rule)
and zrule =
  | RuleZP(ZPat.t, UHExp.block)
  | RuleZE(UHPat.t, zblock);

let bidelimit = (ze: t): t =>
  switch (ze) {
  | CursorE(cursor_side, e) => CursorE(cursor_side, UHExp.bidelimit(e))
  | ParenthesizedZ(_)
  | DeeperE(_, InjZ(_, _))
  | DeeperE(_, ApPaletteZ(_, _, _)) => ze
  | OpSeqZ(_, _, _)
  | DeeperE(_, CaseZE(_, _, _))
  | DeeperE(_, CaseZR(_, _, _))
  | DeeperE(_, CaseZA(_, _, _))
  | DeeperE(_, LamZP(_, _, _))
  | DeeperE(_, LamZA(_, _, _))
  | DeeperE(_, LamZE(_, _, _)) =>
    ParenthesizedZ(DeeperB(NotInHole, BlockZE([], ze)))
  };

exception SkelInconsistentWithOpSeq;

let rec get_err_status_block = (zblock: zblock): err_status =>
  switch (zblock) {
  | CursorB(_, block) => UHExp.get_err_status_block(block)
  | DeeperB(err_status, _) => err_status
  }
and get_err_status_t = (ze: t): err_status =>
  switch (ze) {
  | CursorE(_, e) => UHExp.get_err_status_t(e)
  | ParenthesizedZ(zblock) => get_err_status_block(zblock)
  | OpSeqZ(skel, ze_n, surround) =>
    get_err_status_opseq(skel, ze_n, surround)
  | DeeperE(err, _) => err
  }
and get_err_status_opseq =
    (skel: UHExp.skel_t, ze_n: t, surround: opseq_surround): err_status =>
  switch (skel) {
  | Placeholder(m) =>
    if (m === OperatorSeq.surround_prefix_length(surround)) {
      get_err_status_t(ze_n);
    } else {
      switch (OperatorSeq.surround_nth(m, surround)) {
      | None => raise(SkelInconsistentWithOpSeq)
      | Some(e_m) => UHExp.get_err_status_t(e_m)
      };
    }
  | BinOp(err, _, _, _) => err
  };

let rec set_err_status_block = (err: err_status, zblock: zblock): zblock =>
  switch (zblock) {
  | CursorB(side, block) =>
    CursorB(side, UHExp.set_err_status_block(err, block))
  | DeeperB(_, zblock') => DeeperB(err, zblock')
  }
and set_err_status_t = (err: err_status, ze: t): t =>
  switch (ze) {
  | ParenthesizedZ(zblock) =>
    ParenthesizedZ(set_err_status_block(err, zblock))
  | CursorE(cursor_side, e) =>
    let e = UHExp.set_err_status_t(err, e);
    CursorE(cursor_side, e);
  | OpSeqZ(skel, ze_n, surround) =>
    let (skel, ze_n, surround) =
      set_err_status_opseq(err, skel, ze_n, surround);
    OpSeqZ(skel, ze_n, surround);
  | DeeperE(_, ze') => DeeperE(err, ze')
  }
and set_err_status_opseq =
    (err: err_status, skel: UHExp.skel_t, ze_n: t, surround: opseq_surround)
    : (UHExp.skel_t, t, opseq_surround) =>
  switch (skel) {
  | Placeholder(m) =>
    if (m === OperatorSeq.surround_prefix_length(surround)) {
      let ze_n = set_err_status_t(err, ze_n);
      (skel, ze_n, surround);
    } else {
      switch (OperatorSeq.surround_nth(m, surround)) {
      | None => raise(SkelInconsistentWithOpSeq)
      | Some(e_m) =>
        let e_m = UHExp.set_err_status_t(err, e_m);
        switch (OperatorSeq.surround_update_nth(m, surround, e_m)) {
        | None => raise(SkelInconsistentWithOpSeq)
        | Some(surround) => (skel, ze_n, surround)
        };
      };
    }
  | BinOp(_, op, skel1, skel2) =>
    let skel = Skel.BinOp(err, op, skel1, skel2);
    (skel, ze_n, surround);
  };

let wrap_in_block = (ze: t): zblock => {
  let err_status = get_err_status_t(ze);
  let ze = set_err_status_t(NotInHole, ze);
  set_err_status_block(err_status, DeeperB(NotInHole, BlockZE([], ze)));
};

let rec make_block_inconsistent =
        (u_gen: MetaVarGen.t, zblock: zblock): (zblock, MetaVarGen.t) =>
  switch (zblock) {
  | CursorB(side, block) =>
    let (block, u_gen) = UHExp.make_block_inconsistent(u_gen, block);
    let zblock = CursorB(side, block);
    (zblock, u_gen);
  | DeeperB(_, zblock') =>
    let (u, u_gen) = MetaVarGen.next(u_gen);
    let zblock = DeeperB(InHole(TypeInconsistent, u), zblock');
    (zblock, u_gen);
  }
and make_t_inconsistent = (u_gen: MetaVarGen.t, ze: t): (t, MetaVarGen.t) =>
  switch (ze) {
  | CursorE(cursor_side, e) =>
    let (e, u_gen) = UHExp.make_t_inconsistent(u_gen, e);
    (CursorE(cursor_side, e), u_gen);
  | DeeperE(NotInHole, ze')
  | DeeperE(InHole(WrongLength, _), ze') =>
    let (u, u_gen) = MetaVarGen.next(u_gen);
    let ze = set_err_status_t(InHole(TypeInconsistent, u), ze);
    (ze, u_gen);
  | DeeperE(InHole(TypeInconsistent, _), _) => (ze, u_gen)
  | ParenthesizedZ(zblock) =>
    let (zblock, u_gen) = make_block_inconsistent(u_gen, zblock);
    (ParenthesizedZ(zblock), u_gen);
  | OpSeqZ(skel, ze_n, surround) =>
    let (skel, ze_n, surround, u_gen) =
      make_skel_inconsistent(u_gen, skel, ze_n, surround);
    (OpSeqZ(skel, ze_n, surround), u_gen);
  }
and make_skel_inconsistent =
    (
      u_gen: MetaVarGen.t,
      skel: UHExp.skel_t,
      ze_n: t,
      surround: opseq_surround,
    )
    : (UHExp.skel_t, t, opseq_surround, MetaVarGen.t) =>
  switch (skel) {
  | Placeholder(m) =>
    if (m === OperatorSeq.surround_prefix_length(surround)) {
      let (ze_n, u_gen) = make_t_inconsistent(u_gen, ze_n);
      (skel, ze_n, surround, u_gen);
    } else {
      switch (OperatorSeq.surround_nth(m, surround)) {
      | None => raise(SkelInconsistentWithOpSeq)
      | Some(e_m) =>
        let (e_m, u_gen) = UHExp.make_t_inconsistent(u_gen, e_m);
        switch (OperatorSeq.surround_update_nth(m, surround, e_m)) {
        | None => raise(SkelInconsistentWithOpSeq)
        | Some(surround) => (skel, ze_n, surround, u_gen)
        };
      };
    }
  | BinOp(InHole(TypeInconsistent, _), _, _, _) => (
      skel,
      ze_n,
      surround,
      u_gen,
    )
  | BinOp(NotInHole, op, skel1, skel2)
  | BinOp(InHole(WrongLength, _), op, skel1, skel2) =>
    let (u, u_gen) = MetaVarGen.next(u_gen);
    (
      BinOp(InHole(TypeInconsistent, u), op, skel1, skel2),
      ze_n,
      surround,
      u_gen,
    );
  };

let new_EmptyHole = (u_gen: MetaVarGen.t): (t, MetaVarGen.t) => {
  let (e, u_gen) = UHExp.new_EmptyHole(u_gen);
  (CursorE(Before, e), u_gen);
};

/*
 let rec cursor_on_outer_expr = (ze: t): option((UHExp.t, cursor_side)) =>
   switch (ze) {
   | CursorE(side, e) => Some((UHExp.drop_outer_parentheses(e), side))
   | ParenthesizedZ(ze') => cursor_on_outer_expr(ze')
   | DeeperE(_, _) => None
   };
 */

let empty_zrule = (u_gen: MetaVarGen.t): (zrule, MetaVarGen.t) => {
  let (zp, u_gen) = ZPat.new_EmptyHole(u_gen);
  let (e, u_gen) = UHExp.new_EmptyHole(u_gen);
  let block = UHExp.Block(NotInHole, [], e);
  let zrule = RuleZP(zp, block);
  (zrule, u_gen);
};

let rec erase_block = (zblock: zblock): UHExp.block =>
  switch (zblock) {
  | CursorB(_, block) => block
  | DeeperB(err_status, BlockZL(zlines, e)) =>
    Block(err_status, erase_lines(zlines), e)
  | DeeperB(err_status, BlockZE(lines, ze)) =>
    Block(err_status, lines, erase(ze))
  }
and erase_lines = (zlis: zlines): UHExp.lines =>
  ZList.erase(zlis, erase_line)
and erase_line = (zli: zline): UHExp.line =>
  switch (zli) {
  | CursorL(_, li) => li
  | DeeperL(zli') => erase_line'(zli')
  }
and erase_line' = (zli': zline'): UHExp.line =>
  switch (zli') {
  | ExpLineZ(ze) => ExpLine(erase(ze))
  | LetLineZP(zp, ann, block) => LetLine(ZPat.erase(zp), ann, block)
  | LetLineZA(p, zann, block) => LetLine(p, Some(ZTyp.erase(zann)), block)
  | LetLineZE(p, ann, zblock) => LetLine(p, ann, erase_block(zblock))
  }
and erase = (ze: t): UHExp.t =>
  switch (ze) {
  | CursorE(_, e) => e
  | DeeperE(err_state, ze') =>
    let e' = erase'(ze');
    Tm(err_state, e');
  | ParenthesizedZ(zblock) => UHExp.Parenthesized(erase_block(zblock))
  | OpSeqZ(skel, ze', surround) =>
    let e = erase(ze');
    OpSeq(skel, OperatorSeq.opseq_of_exp_and_surround(e, surround));
  }
and erase' = (ze: t'): UHExp.t' =>
  switch (ze) {
  | LamZP(zp, ann, block) => Lam(ZPat.erase(zp), ann, block)
  | LamZA(p, zann, block) => Lam(p, Some(ZTyp.erase(zann)), block)
  | LamZE(p, ann, zblock) => Lam(p, ann, erase_block(zblock))
  | InjZ(side, zblock) => Inj(side, erase_block(zblock))
  | CaseZE(zblock, rules, ann) => Case(erase_block(zblock), rules, ann)
  | CaseZR(block, zrules, ann) =>
    Case(block, ZList.erase(zrules, erase_rule), ann)
  | CaseZA(e1, rules, zann) =>
    UHExp.Case(e1, rules, Some(ZTyp.erase(zann)))
  | ApPaletteZ(palette_name, serialized_model, zpsi) =>
    let psi = ZSpliceInfo.erase(zpsi, ((ty, z)) => (ty, erase_block(z)));
    ApPalette(palette_name, serialized_model, psi);
  }
and erase_rule = (zr: zrule): UHExp.rule =>
  switch (zr) {
  | RuleZP(zp, block) => Rule(ZPat.erase(zp), block)
  | RuleZE(p, zblock) => Rule(p, erase_block(zblock))
  };

let rec is_before_block = (zblock: zblock): bool =>
  switch (zblock) {
  | CursorB(Before, _) => true
  | CursorB(_, _) => false
  | DeeperB(_, BlockZL(zlines, _)) => is_before_lines(zlines)
  | DeeperB(_, BlockZE([], ze)) => is_before_exp(ze)
  | DeeperB(_, BlockZE(_, _)) => false
  }
and is_before_lines = ((prefix, zline, _): zlines): bool =>
  switch (prefix) {
  | [] => is_before_line(zline)
  | _ => false
  }
and is_before_line = (zline: zline): bool =>
  switch (zline) {
  | CursorL(_, EmptyLine) => true
  | CursorL(Before, _) => true
  | CursorL(_, _) => false
  | DeeperL(zline') => is_before_line'(zline')
  }
and is_before_line' = (zline': zline'): bool =>
  switch (zline') {
  | ExpLineZ(ze) => is_before_exp(ze)
  | LetLineZP(_, _, _)
  | LetLineZA(_, _, _)
  | LetLineZE(_, _, _) => false
  }
and is_before_exp = (ze: t): bool =>
  switch (ze) {
  | CursorE(Before, _) => true
  | CursorE(_, _) => false
  | ParenthesizedZ(_) => false
  | OpSeqZ(_, ze1, EmptyPrefix(_)) => is_before_exp(ze1)
  | OpSeqZ(_, _, _) => false
  | DeeperE(_, LamZP(_, _, _))
  | DeeperE(_, LamZA(_, _, _))
  | DeeperE(_, LamZE(_, _, _)) => false
  | DeeperE(_, InjZ(_, _)) => false
  | DeeperE(_, CaseZE(_, _, _))
  | DeeperE(_, CaseZR(_, _, _))
  | DeeperE(_, CaseZA(_, _, _)) => false
  | DeeperE(_, ApPaletteZ(_, _, _)) => false
  };

let rec is_after_block = (zblock: zblock): bool =>
  switch (zblock) {
  | CursorB(After, _) => true
  | CursorB(_, _) => false
  | DeeperB(_, BlockZL(_, _)) => false
  | DeeperB(_, BlockZE(_, ze)) => is_after_exp(ze)
  }
and is_after_line = (zli: zline): bool =>
  switch (zli) {
  | CursorL(_, EmptyLine) => true
  | CursorL(After, _) => true
  | CursorL(_, _) => false
  | DeeperL(zli') => is_after_line'(zli')
  }
and is_after_line' = (zli': zline'): bool =>
  switch (zli') {
  | ExpLineZ(ze) => is_after_exp(ze)
  | LetLineZP(_, _, _)
  | LetLineZA(_, _, _) => false
  | LetLineZE(_, _, zblock) => is_after_block(zblock)
  }
and is_after_exp = (ze: t): bool =>
  switch (ze) {
  | CursorE(After, _) => true
  | CursorE(_, _) => false
  | ParenthesizedZ(_) => false
  | OpSeqZ(_, ze1, EmptySuffix(_)) => is_after_exp(ze1)
  | OpSeqZ(_, _, _) => false
  | DeeperE(_, LamZP(_, _, _))
  | DeeperE(_, LamZA(_, _, _)) => false
  | DeeperE(_, LamZE(_, _, zblock)) => is_after_block(zblock)
  | DeeperE(_, InjZ(_, _)) => false
  | DeeperE(_, CaseZE(_, _, _))
  | DeeperE(_, CaseZR(_, _, _)) => false
  | DeeperE(_, CaseZA(_, _, zann)) => ZTyp.is_after(zann)
  | DeeperE(_, ApPaletteZ(_, _, _)) => false
  };

let rec place_before_block = (block: UHExp.block): zblock =>
  switch (block) {
  | Block(err_status, [], e) =>
    DeeperB(err_status, BlockZE([], place_before_exp(e)))
  | Block(err_status, [line, ...lines], e) =>
    let zline = place_before_line(line);
    let zlines = ([], zline, lines);
    DeeperB(err_status, BlockZL(zlines, e));
  }
and place_before_line = (line: UHExp.line): zline =>
  switch (line) {
  | EmptyLine
  | LetLine(_, _, _) => CursorL(Before, line)
  | ExpLine(e) => DeeperL(ExpLineZ(place_before_exp(e)))
  }
and place_before_exp = (e: UHExp.t): t =>
  switch (e) {
  | OpSeq(skel, opseq) =>
    let (e1, suffix) = OperatorSeq.split0(opseq);
    let ze1 = place_before_exp(e1);
    let surround = OperatorSeq.EmptyPrefix(suffix);
    OpSeqZ(skel, ze1, surround);
  | EmptyHole(_)
  | Parenthesized(_)
  | Tm(_, Var(_, _))
  | Tm(_, Lam(_, _, _))
  | Tm(_, NumLit(_))
  | Tm(_, BoolLit(_))
  | Tm(_, Inj(_, _))
  | Tm(_, Case(_, _, _))
  | Tm(_, ListNil)
  | Tm(_, ApPalette(_, _, _)) => CursorE(Before, e)
  };

let rec place_after_block =
        (Block(err_status, lines, e): UHExp.block): zblock =>
  DeeperB(err_status, BlockZE(lines, place_after_exp(e)))
and place_after_line = (line: UHExp.line): zline =>
  switch (line) {
  | EmptyLine => CursorL(After, line)
  | ExpLine(e) => DeeperL(ExpLineZ(place_after_exp(e)))
  | LetLine(p, ann, block) =>
    DeeperL(LetLineZE(p, ann, place_after_block(block)))
  }
and place_after_exp = (e: UHExp.t): t =>
  switch (e) {
  | OpSeq(skel, opseq) =>
    let (en, prefix) = OperatorSeq.split_tail(opseq);
    let zen = place_after_exp(en);
    let surround = OperatorSeq.EmptySuffix(prefix);
    OpSeqZ(skel, zen, surround);
  | EmptyHole(_)
  | Parenthesized(_) => CursorE(After, e)
  | Tm(_, Case(e, rules, Some(ty))) =>
    DeeperE(NotInHole, CaseZA(e, rules, ZTyp.place_after(ty)))
  | Tm(_, Case(_, _, None))
  | Tm(_, Var(_, _))
  | Tm(_, Lam(_, _, _))
  | Tm(_, NumLit(_))
  | Tm(_, BoolLit(_))
  | Tm(_, Inj(_, _))
  | Tm(_, ListNil)
  | Tm(_, ApPalette(_, _, _)) => CursorE(After, e)
  };
