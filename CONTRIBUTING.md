# Contributing

The portable C++20 engine lives in `native/physics`, `native/rules` and
`native/render`; Windows and Linux services live in `native/platform`.
`native/editions/classic` and `native/editions/enhanced` supply edition policies.
Keep Classic behavior and deliberate Enhanced changes separate.

Source lists are explicit in `native/core-sources.txt` and each edition's
`sources.txt`. Keep them current when adding translation units. Format C++ with
`clang-format --style=file` using the supplied `.clang-format`, and use LF line
endings as specified in `.gitattributes`.

Run `python tools/test.py` before submitting a change. See the README for build
prerequisites. Add tests using synthetic inputs; never add game media,
screenshots, extracted artwork or sound samples.

Describe the behavior changed and how you tested it in your pull request.
