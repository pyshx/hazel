module Js = Js_of_ocaml.Js;
module Exn = Base.Exn;
module Dom_html = Js_of_ocaml.Dom_html;
module Dom = Js_of_ocaml.Dom;
module Vdom = Virtual_dom.Vdom;

Logger.init_log();

let () = {
  // Handle exceptions via a Monitor
  // See <https://github.com/janestreet/incr_dom/blob/master/example/monitor>
  // <https://github.com/janestreet/async_js/blob/master/src/async_js0.ml>
  let monitor = Async_kernel.Monitor.create();
  let _ =
    Async_kernel.Monitor.detach_and_iter_errors(
      monitor,
      ~f=exn => {
        let exn =
          switch (Async_kernel.Monitor.extract_exn(exn)) {
          | Js.Error(err) => `Js(err)
          | exn => `Exn(exn)
          };
        let message =
          switch (exn) {
          | `Js(err) =>
            let backtrace =
              switch (Js.Optdef.to_option(err##.stack)) {
              | Some(stack) => Js.to_string(stack)
              | None => "No backtrace found"
              };
            Js.to_string(err##.message) ++ backtrace;
          | `Exn(exn) => Exn.to_string(exn)
          };
        print_endline(message);

        let err_box = JSUtil.force_get_elem_by_id("error-message");
        err_box##.classList##add(Js.string("visible"));
        /*
         TODO: Hannah Not sure if just making visible and invisible is really the
         best way to do this since then you will have invisible elements just hanging
         around (but having trouble making the thing below work)
         let dom =
           Vdom.Node.to_dom(
             Vdom.Node.body(
               [],
               [Vdom.Node.h2([], [Vdom.Node.text("Error!")])],
             ),
           );
         let dom =
           Dom.document##createElement(Js.string("some-error-message"));
         let current_body =
           Dom_html.document##.body##replaceChild(err_box, dom);
         ();*/
      },
    );

  Async_kernel.Async_kernel_scheduler.within(~monitor, ()
    // Start the main app.
    // See <https://github.com/janestreet/incr_dom/blob/master/src/start_app.mli>.
    =>
      Incr_dom.Start_app.start(
        (module Hazel),
        ~debug=false,
        ~bind_to_element_with_id="container",
        ~initial_model=Model.init(),
      )
    );
};
