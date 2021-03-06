open Jest;

open BsPuppeteer;

open Expect;

let seconds = v => v * 1000;

let getElementValueJs: Dom.element => string = [%raw
  {| function (element) { return element.value; } |}
];

let getLengthOfElementsJs = [%raw
  {| function (elements) { return elements.length; } |}
];

let getElementOuterHTMLJs: Dom.element => string = [%raw
  {| function (element) { return element.outerHTML; } |}
];

let getElementOuterHTMLJsPromise: Dom.element => Js.Promise.t(string) = [%raw
  {| function (element) { return Promise.resolve(element.outerHTML); } |}
];

let fixturesPath =
  Node.Path.resolve(
    [%bs.node __dirname] |> Js.Option.getWithDefault(""),
    "../../../__tests__/fixtures/",
  );

let testPagePath = Node.Path.resolve(fixturesPath, "./testPage.html");

let testPageJsPath = Node.Path.resolve(fixturesPath, "./testPage.js");

let testPageCssPath = Node.Path.resolve(fixturesPath, "./testPage.css");

let testPageContent = Node.Fs.readFileAsUtf8Sync(testPagePath);

let noSandbox =
  Puppeteer.makeLaunchOptions(
    ~args=[|"--no-sandbox", "--disable-setuid-sandbox"|],
    (),
  );

describe("Puppeteer", () => {
  test("executablePath", () =>
    Puppeteer.executablePath() |> expect |> toContainString("chromium")
  );
  test("defaultArgs()", () =>
    Puppeteer.defaultArgs() |> Array.length |> expect |> toBeGreaterThan(0)
  );
});

describe("BrowserFetcher", () => {
  let browserFetcher = ref(BrowserFetcher.empty());
  beforeAll(() =>
    browserFetcher :=
      Puppeteer.createBrowserFetcher(
        ~options=Puppeteer.makeBrowserFetcherOptions(),
        (),
      )
  );
  testPromise("canDownload", () =>
    Js.Promise.(
      browserFetcher^
      |> BrowserFetcher.canDownload(_, "533271")
      |> then_(boolean => boolean |> expect |> toBe(true) |> resolve)
    )
  );
  testPromise("download", ~timeout=30 |> seconds, () =>
    Js.Promise.(
      browserFetcher^
      |> BrowserFetcher.download(~revision="533271")
      |> then_(info => info##revision |> expect |> toBe("533271") |> resolve)
    )
  );
  testPromise("localRevisions", () =>
    Js.Promise.(
      browserFetcher^
      |> BrowserFetcher.localRevisions
      |> then_(revisions => revisions |> expect |> toHaveLength(2) |> resolve)
    )
  );
  test("platform", () =>
    browserFetcher^
    |> BrowserFetcher.platform
    |> expect
    |> toEqual(Some(`linux))
  );
  testPromise("remove", ~timeout=30 |> seconds, () =>
    Js.Promise.(
      browserFetcher^
      |> BrowserFetcher.download(~revision="533273")
      |> then_(_info =>
           browserFetcher^ |> BrowserFetcher.remove(_, "533273")
         )
      |> then_(() => pass |> resolve)
      |> catch(_error =>
           fail("the revision has not been downloaded") |> resolve
         )
    )
  );
  test("revisionInfo", () => {
    let revisionInfo =
      browserFetcher^ |> BrowserFetcher.revisionInfo(_, "533271");
    revisionInfo##revision |> expect |> toBe("533271") |> ignore;
    revisionInfo##executablePath
    |> expect
    |> toContainString("chromium")
    |> ignore;
    revisionInfo##folderPath
    |> expect
    |> toContainString("chromium")
    |> ignore;
    revisionInfo##local |> expect |> toBe(true) |> ignore;
    revisionInfo##url
    |> expect
    |> toContainString("https://storage.googleapis.com/");
  });
});

describe("Browser", () => {
  let browser = ref(Browser.empty());
  beforeAllPromise(() =>
    Js.Promise.(
      Puppeteer.launch(~options=noSandbox, ())
      |> then_(res => {
           browser := res;
           resolve();
         })
    )
  );
  test("wsEndpoint()", () =>
    Browser.wsEndpoint(browser^)
    |> expect
    |> toContainString("ws://127.0.0.1:")
  );
  testPromise("userAgent()", () =>
    Js.Promise.(
      Browser.userAgent(browser^)
      |> then_(userAgent =>
           userAgent |> expect |> toContainString("HeadlessChrome") |> resolve
         )
    )
  );
  testPromise("version()", () =>
    Js.Promise.(
      Browser.version(browser^)
      |> then_(version =>
           version
           |> expect
           |> toMatchRe([%bs.re "/^HeadlessChrome/"])
           |> resolve
         )
    )
  );
  afterAllPromise(() => Browser.close(browser^));
});

describe("Page", () => {
  let browser = ref(Browser.empty());
  let page = ref(Page.empty());
  beforeAllPromise(() =>
    Js.Promise.(
      Puppeteer.launch(~options=noSandbox, ())
      |> then_(res => {
           browser := res;
           browser^ |> Browser.newPage;
         })
      |> then_(res => {
           page := res;
           resolve();
         })
    )
  );
  testPromise("$()", () =>
    Js.Promise.(
      Page.selectOne(page^, ~selector="body")
      |> then_(elementHandle =>
           elementHandle |> expect |> ExpectJs.toBeTruthy |> resolve
         )
    )
  );
  testPromise("content()", () =>
    Js.Promise.(
      Page.content(page^)
      |> then_(content =>
           expect(content)
           |> ExpectJs.toBe("<html><head></head><body></body></html>")
           |> resolve
         )
    )
  );
  testPromise("$$()", () =>
    Js.Promise.(
      Page.selectAll(page^, ~selector="html,body")
      |> then_(elementHandles =>
           expect(elementHandles) |> ExpectJs.toHaveLength(2) |> resolve
         )
    )
  );
  testPromise("$x", () =>
    Js.Promise.(
      Page.selectXPath(page^, ~xpath="/html/body")
      |> then_(elementHandles =>
           expect(elementHandles) |> ExpectJs.toHaveLength(1) |> resolve
         )
    )
  );
  testPromise("$$eval()", () =>
    Js.Promise.(
      page^
      |> Page.selectAllEval("html,body", getLengthOfElementsJs)
      |> then_(length => length |> expect |> toBe(2.0) |> resolve)
    )
  );
  testPromise("$eval() with 0 args", () =>
    Js.Promise.(
      page^
      |> Page.selectOneEval("html", getElementOuterHTMLJs)
      |> then_(html =>
           html
           |> expect
           |> toBe("<html><head></head><body></body></html>")
           |> resolve
         )
    )
  );
  testPromise("$eval() with 0 args returning a promise", () =>
    Js.Promise.(
      page^
      |> Page.selectOneEvalPromise("html", getElementOuterHTMLJsPromise)
      |> then_(h =>
           h
           |> expect
           |> toBe("<html><head></head><body></body></html>")
           |> resolve
         )
    )
  );
  testPromise("$eval() with 1 arg", () =>
    Js.Promise.(
      page^
      |> Page.setContent(testPageContent)
      |> then_(() =>
           page^
           |> Page.selectOneEval1(
                "input",
                [%raw
                  {| function (el, prop) { return el.getAttribute(prop); } |}
                ],
                "id",
              )
         )
      |> then_(id => id |> expect |> toBe("input") |> resolve)
    )
  );
  testPromise("click()", () =>
    Js.Promise.(
      page^ |> Page.click("body", ()) |> then_(() => pass |> resolve)
    )
  );
  testPromise("goto()", () =>
    Js.Promise.(
      browser^
      |> Browser.newPage
      |> then_(page => {
           let options = Navigation.makeOptions(~timeout=25000., ());
           page |> Page.goto("file://" ++ testPagePath, ~options, ());
         })
      |> then_(res => res |> Js.Null.getExn |> Response.text)
      |> then_(text =>
           text
           |> expect
           |> toContainString("<title>Test Page</title>")
           |> resolve
         )
    )
  );
  testPromise("screenshot()", () =>
    Js.Promise.(
      page^
      |> Page.screenshot()
      |> then_(buf =>
           buf
           |> Js_typed_array.ArrayBuffer.byteLength
           |> expect
           |> toBeGreaterThanOrEqual(3236)
           |> resolve
         )
    )
  );
  testPromise("waitForSelector()", () =>
    Js.Promise.(
      page^
      |> Page.waitForSelector("body", ())
      |> then_(() => pass |> resolve)
    )
  );
  testPromise("waitForXPath()", () =>
    Js.Promise.(
      page^
      |> Page.waitForXPath(
           ~xpath="/html/body",
           ~options=Page.makeSelectorOptions(~timeout=100., ()),
           (),
         )
      |> then_(elementHandle =>
           elementHandle |> expect |> ExpectJs.toBeTruthy |> resolve
         )
    )
  );
  testPromise("setExtraHTTPHeaders", () =>
    Js.Promise.(
      page^
      |> Page.setExtraHTTPHeaders(
           ~headers=Js.Dict.fromList([("extra-http-header", "header01")]),
           (),
         )
      /* TODO: Better way to verify extra HTTP headers */
      |> then_(() => pass |> resolve)
    )
  );
  testPromise("type()", () =>
    Js.Promise.(
      browser^
      |> Browser.newPage
      |> then_(page =>
           page
           |> Page.setContent(testPageContent)
           |> then_(() => page |> Page.type_("#input", "hello world", ()))
           |> then_(() =>
                page |> Page.selectOneEval("#input", getElementValueJs)
              )
         )
      |> then_(value => value |> expect |> toBe("hello world") |> resolve)
    )
  );
  testPromise("addScriptTag()", () =>
    Js.Promise.(
      browser^
      |> Browser.newPage
      |> then_(page =>
           page
           |> Page.addScriptTag(
                Page.makeTagOptions(~path=testPageJsPath, ()),
              )
           |> then_(_elementHandle => Page.content(page))
           |> then_(content =>
                Page.close(page)
                |> then_(() =>
                     content
                     |> expect
                     |> toContainString("// This is \"testPage.js\"")
                     |> resolve
                   )
              )
         )
    )
  );
  testPromise("addStyleTag()", () =>
    Js.Promise.(
      browser^
      |> Browser.newPage
      |> then_(page =>
           page
           |> Page.addStyleTag(
                Page.makeTagOptions(~path=testPageCssPath, ()),
              )
           |> then_(_elementHandle => Page.content(page))
           |> then_(content =>
                Page.close(page)
                |> then_(() =>
                     content
                     |> expect
                     |> toContainString("/* This is \"testPage.css\" */")
                     |> resolve
                   )
              )
         )
    )
  );
  testPromise("authenticate()", () =>
    Js.Promise.(
      page^
      |> Page.authenticate(
           Js.Null.return({"username": "foo", "password": "bar"}),
         )
      |> then_(() => pass |> resolve)
    )
  );
  testPromise("cookies()", () =>
    Js.Promise.(
      page^
      |> Page.setCookie([|
           Page.makeCookie(
             ~name="foo",
             ~value="bar",
             ~url="http://localhost",
             (),
           ),
         |])
      |> then_(() => page^ |> Page.cookies([|"http://localhost"|]))
      |> then_(cookies => cookies |> expect |> toHaveLength(1) |> resolve)
    )
  );
  testPromise("setCookie()", () =>
    Js.Promise.(
      page^
      |> Page.setCookie([|
           Page.makeCookie(
             ~name="foo",
             ~value="bar",
             ~url="http://localhost",
             (),
           ),
           Page.makeCookie(
             ~name="foo2",
             ~value="bar2",
             ~url="http://localhost2",
             (),
           ),
         |])
      |> then_(() =>
           page^ |> Page.cookies([|"http://localhost", "http://localhost2"|])
         )
      |> then_(cookies => cookies |> expect |> toHaveLength(2) |> resolve)
    )
  );
  testPromise("deleteCookie()", () =>
    Js.Promise.(
      page^
      |> Page.setCookie([|
           Page.makeCookie(
             ~name="foo",
             ~value="bar",
             ~url="http://localhost",
             (),
           ),
         |])
      |> then_(() => page^ |> Page.deleteCookie([||]))
      |> then_(() => page^ |> Page.cookies([||]))
      |> then_(cookies => cookies |> expect |> toHaveLength(0) |> resolve)
    )
  );
  testPromise("emulate()", () =>
    Js.Promise.(
      page^
      |> Page.emulate({
           "viewport": {
             "width": 320,
             "height": 480,
             "deviceScaleFactor": 2,
             "isMobile": true,
             "hasTouch": true,
             "isLandscape": true,
           },
           "userAgent": "",
         })
      |> then_(() =>
           page^
           |> Page.viewport()
           |> expect
           |> ExpectJs.toMatchObject({
                "width": 320,
                "height": 480,
                "deviceScaleFactor": 2,
                "isMobile": true,
                "hasTouch": true,
                "isLandscape": true,
              })
           |> resolve
         )
    )
  );
  testPromise("emulateMedia()", () =>
    Js.Promise.(
      page^ |> Page.emulateMedia(`print) |> then_(() => pass |> resolve)
    )
  );
  testPromise("emulateMediaDisable()", () =>
    Js.Promise.(
      page^ |> Page.emulateMediaDisable |> then_(() => pass |> resolve)
    )
  );
  testPromise("evaluate()", () =>
    Js.Promise.(
      {
        let eval = () => "ok";
        page^ |> Page.evaluate(eval);
      }
      |> then_(res => res |> expect |> toBe("ok") |> resolve)
    )
  );
  testPromise("evaluate() with 1 arg", () =>
    Js.Promise.(
      {
        let eval = arg => arg ++ "iedoke";
        page^ |> Page.evaluate1(eval, "ok");
      }
      |> then_(res => res |> expect |> toBe("okiedoke") |> resolve)
    )
  );
  testPromise("evaluate1() and a curried function", () =>
    Js.Promise.(
      {
        let eval = (arg1, arg2) => arg1 ++ " " ++ arg2;
        page^ |> Page.evaluate1(eval("hello"), "world");
      }
      |> then_(res => res |> expect |> toBe("hello world") |> resolve)
    )
  );
  testPromise("evaluatePromise1() with a curried function", () =>
    Js.Promise.(
      {
        let eval = (arg1, arg2) => resolve(arg1 ++ " " ++ arg2);
        page^ |> Page.evaluatePromise1(eval("hello"), "world");
      }
      |> then_(res => res |> expect |> toBe("hello world") |> resolve)
    )
  );
  testPromise("evaluatePromise2() with a curried function", () =>
    Js.Promise.(
      {
        let eval = (arg1, arg2, arg3) => resolve(arg1 ++ " " ++ arg2 ++ arg3);
        page^ |> Page.evaluatePromise2(eval("hello"), "world", "!");
      }
      |> then_(res => res |> expect |> toBe("hello world!") |> resolve)
    )
  );
  testPromise("evaluate() with 2 args", () =>
    Js.Promise.(
      {
        let eval = (arg1, arg2) =>
          (arg1 |> String.length |> Js.Int.toString) ++ arg1 ++ " " ++ arg2;
        page^ |> Page.evaluate2(eval, "hello", "world");
      }
      |> then_(res => res |> expect |> toBe("5hello world") |> resolve)
    )
  );
  testPromise("evaluateString()", () => {
    let getTitleStr = {| document.getElementsByTagName("title")[0].innerHTML; |};
    page^
    |> Page.setContent(testPageContent)
    |> Js.Promise.then_(() =>
         page^
         |> Page.evaluateString(getTitleStr)
         |> Js.Promise.then_(title =>
              title |> expect |> toBe("Test Page") |> Js.Promise.resolve
            )
       );
  });
  testPromise("evaluateHandle()", () =>
    Js.Promise.(
      {
        let eval = () => [%raw {| document |}];
        page^ |> Page.evaluateHandle(eval);
      }
      |> then_(jsHandler =>
           jsHandler |> expect |> ExpectJs.toBeTruthy |> resolve
         )
    )
  );
  testPromise("pdf()", () =>
    Js.Promise.(
      page^
      |> Page.pdf(
           Page.makePDFOptions(
             ~scale=1,
             ~displayHeaderFooter=true,
             ~headerTemplate="[[header]]",
             ~footerTemplate="[[footer]]",
             ~printBackground=true,
             ~landscape=true,
             ~pageRanges="",
             ~format=`A0,
             ~width=10.0 |> Unit.cm,
             ~height=200.0 |> Unit.mm,
             ~margin=
               Page.makeMargin(
                 ~top=0.1 |> Unit.cm,
                 ~right=10.0 |> Unit.px,
                 ~bottom=1.0 |> Unit.mm,
                 ~left=0.01 |> Unit.in_,
                 (),
               ),
             (),
           ),
         )
      |> then_(buffer =>
           buffer
           |> Js_typed_array.ArrayBuffer.byteLength
           |> expect
           |> toBeGreaterThan(20000)
           |> Js.Promise.resolve
         )
    )
  );
  test("target()", () =>
    page^ |> Page.target |> Target.url |> expect |> toBe("about:blank")
  );
  test("coverage", () =>
    page^ |> Page.coverage |> expect |> ExpectJs.toBeTruthy
  );
  afterAllPromise(() =>
    Js.Promise.(Page.close(page^) |> then_(() => Browser.close(browser^)))
  );
});

describe("ElementHandle", () => {
  let browser = ref(Browser.empty());
  let page = ref(Page.empty());
  let elementHandle = ref(ElementHandle.empty());
  beforeAllPromise(() =>
    Js.Promise.(
      Puppeteer.launch(~options=noSandbox, ())
      |> then_(res => {
           browser := res;
           browser^ |> Browser.newPage;
         })
      |> then_(res => {
           page := res;
           page^ |> Page.goto("file://" ++ testPagePath, ());
         })
      |> then_(_resp => Page.selectOne(page^, ~selector="#iframe"))
      |> then_(res =>
           switch (res |> Js.nullToOption) {
           | Some(v) =>
             elementHandle := v;
             resolve();
           | None =>
             reject(Js.Exn.raiseError("failed to initial an elementhandle"))
           }
         )
    )
  );
  testPromise("contentFrame()", () =>
    Js.Promise.(
      elementHandle^
      |> ElementHandle.contentFrame
      |> then_(frame =>
           switch (frame |> Js.nullToOption) {
           | Some(v) => v |> expect |> ExpectJs.toBeTruthy |> resolve
           | None =>
             Js.Exn.raiseError("failed to get frame from contentFrame")
             |> reject
           }
         )
    )
  );
  afterAllPromise(() =>
    Js.Promise.(Page.close(page^) |> then_(() => Browser.close(browser^)))
  );
});

describe("Target", () => {
  let browser = ref(Browser.empty());
  let target = ref(Target.empty());
  beforeAllPromise(() =>
    Js.Promise.(
      Puppeteer.launch(~options=noSandbox, ())
      |> then_(res => {
           browser := res;
           res |> Browser.newPage;
         })
      |> then_(page => {
           target := page |> Page.target;
           target |> resolve;
         })
    )
  );
  testPromise("page", () =>
    Js.Promise.(
      target^
      |> Target.page
      |> then_(page => page |> expect |> ExpectJs.toBeTruthy |> resolve)
    )
  );
  test("type", () =>
    (
      switch (target^ |> Target.type_) {
      | Some(t) => t === `page
      | None => false
      }
    )
    |> expect
    |> toBe(true)
  );
  test("url", () =>
    target^ |> Target.url |> expect |> toBe("about:blank")
  );
  testPromise("createCDPSession", () =>
    Js.Promise.(
      target^
      |> Target.createCDPSession
      |> then_(session => session |> expect |> ExpectJs.toBeTruthy |> resolve)
    )
  );
  afterAllPromise(() => browser^ |> Browser.close);
});

describe("CDPSession", () => {
  let browser = ref(Browser.empty());
  beforeAllPromise(() =>
    Js.Promise.(
      Puppeteer.launch(~options=noSandbox, ())
      |> then_(res => {
           browser := res;
           res |> resolve;
         })
    )
  );
  testPromise("detach", () =>
    Js.Promise.(
      browser^
      |> Browser.newPage
      |> then_(page => page |> Page.target |> Target.createCDPSession)
      |> then_(session =>
           session
           |> CDPSession.detach
           |> then_(() =>
                session
                |> CDPSession.send(~method_="Animation.getPlaybackRate")
              )
           |> then_(_res =>
                failwith(
                  "expect with exception: Error: Protocol error (Animation.getPlaybackRate): Session closed. Most likely the page has been closed.",
                )
                |> resolve
              )
           |> catch(_err => pass |> resolve)
         )
    )
  );
  testPromise("send", () =>
    Js.Promise.(
      browser^
      |> Browser.newPage
      |> then_(page => page |> Page.target |> Target.createCDPSession)
      |> then_(session =>
           session
           |> CDPSession.send(
                ~method_="Animation.setPlaybackRate",
                ~params={"playbackRate": 3.1415926535},
              )
           |> then_(_res =>
                session
                |> CDPSession.send(~method_="Animation.getPlaybackRate")
              )
           |> then_(res =>
                res##playbackRate |> expect |> toBe(3.1415926535) |> resolve
              )
         )
    )
  );
  afterAllPromise(() => browser^ |> Browser.close);
});

describe("Coverage", () => {
  let browser = ref(Browser.empty());
  beforeAllPromise(() =>
    Js.Promise.(
      Puppeteer.launch(~options=noSandbox, ())
      |> then_(res => {
           browser := res;
           res |> resolve;
         })
    )
  );
  describe("startJSCoverage, stopJSCoverage", () => {
    let report = ref([||]);
    beforeAllPromise(() =>
      Js.Promise.(
        browser^
        |> Browser.newPage
        |> then_(page => {
             let coverage = page |> Page.coverage;
             coverage
             |> Coverage.startJSCoverage(
                  ~options=
                    Coverage.makeJSCoverageOptions(
                      ~resetOnNavigation=true,
                      (),
                    ),
                )
             |> then_(() => page |> Page.goto("file://" ++ testPagePath, ()))
             |> then_(_res => coverage |> Coverage.stopJSCoverage)
             |> then_(res => {
                  report := res;
                  res |> resolve;
                });
           })
      )
    );
    test("report.ranges[0]", () =>
      {
        let ranges = report^[0]##ranges;
        ranges[0];
      }
      |> expect
      |> ExpectJs.toMatchObject({"end": 21, "start": 0})
    );
    test("report.ranges[1]", () =>
      {
        let ranges = report^[0]##ranges;
        ranges[1];
      }
      |> expect
      |> ExpectJs.toMatchObject({"end": 65, "start": 39})
    );
    test("report.text", () =>
      report^[0]##text
      |> expect
      |> toBe(
           {j|
    function foo() {function bar() { } console.log(1); } foo(); |j},
         )
    );
    test("report.url", () =>
      report^[0]##url |> expect |> toContainString("fixtures/testPage.html")
    );
  });
  describe("startCSSCoverage, stopCSSCoverage", () => {
    let report = ref([||]);
    beforeAllPromise(() =>
      Js.Promise.(
        browser^
        |> Browser.newPage
        |> then_(page => {
             let coverage = page |> Page.coverage;
             coverage
             |> Coverage.startCSSCoverage(
                  ~options=
                    Coverage.makeCSSCoverageOptions(
                      ~resetOnNavigation=true,
                      (),
                    ),
                )
             |> then_(() => page |> Page.goto("file://" ++ testPagePath, ()))
             |> then_(_res => coverage |> Coverage.stopCSSCoverage)
             |> then_(res => {
                  report := res;
                  res |> resolve;
                });
           })
      )
    );
    test("report.ranges[0]", () =>
      {
        let ranges = report^[0]##ranges;
        ranges[0];
      }
      |> expect
      |> ExpectJs.toMatchObject({"end": 30, "start": 7})
    );
    test("report.text", () =>
      report^[0]##text
      |> expect
      |> toBe(
           {j|
      input { color: green; }
      a { color: blue; }
    |j},
         )
    );
    test("report.url", () =>
      report^[0]##url |> expect |> toContainString("fixtures/testPage.html")
    );
  });
  afterAllPromise(() => browser^ |> Browser.close);
});
