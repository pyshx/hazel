open Virtual_dom;

/**
 * Strategy Guide at the cursor for expression holes.
 */
let exp_hole_view:
  (
    ~inject: ModelAction.t => Vdom.Event.t,
    Settings.CursorInspector.t,
    CursorInfo.t
  ) =>
  Vdom.Node.t;

let list_vars_view: VarCtx.t => list(Vdom.Node.t);

/**
 * Strategy Guide at the cursor for rules.
 */
let rules_view: CursorInfo.t => option(Vdom.Node.t);

/**
 * Strategy Guide at the cursor for lines.
 */
let lines_view: unit => Vdom.Node.t;