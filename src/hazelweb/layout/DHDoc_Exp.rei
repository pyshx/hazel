let mk:
  (
    ~settings: Settings.Evaluation.t,
    ~parenthesize: bool=?,
    ~enforce_inline: bool,
    ~selected_hole_instance: option(NodeInstance.t),
    DHExp.t
  ) =>
  DHDoc.t;